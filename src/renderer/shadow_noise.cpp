#include "renderer/shadow_noise.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <vector>

#include "core/log.h"

namespace kc::shadow_noise {

static std::mutex g_mutex;  // the texture can be created on a loading thread
static ID3D11Texture2D* g_tex;
static std::vector<std::vector<int16_t>> g_original;  // per mip, tightly packed x,y pairs
static std::vector<std::vector<int16_t>> g_pending;   // captured by adjust_texture, claimed by on_created
static UINT g_size;
static bool g_active, g_dirty;  // render thread
static float g_angle;
static int g_coarse = 1, g_uploaded_coarse = 1;  // render thread

bool found() {
    std::lock_guard lock(g_mutex);
    return g_tex != nullptr;
}

static bool is_noise(const D3D11_TEXTURE2D_DESC& d) {
    return d.Format == DXGI_FORMAT_R16G16_SNORM && d.Width == 64 && d.Height == 64 && d.ArraySize == 1 &&
           d.MipLevels >= 1 && d.BindFlags == D3D11_BIND_SHADER_RESOURCE && d.SampleDesc.Count == 1;
}

bool adjust_texture(const D3D11_TEXTURE2D_DESC* in, const D3D11_SUBRESOURCE_DATA* init, D3D11_TEXTURE2D_DESC* out) {
    if (!in || !is_noise(*in))
        return false;
    if (!init) {
        logf("shadow_noise: 64x64 R16G16_SNORM texture created without data, left alone");
        return false;
    }
    std::vector<std::vector<int16_t>> mips(in->MipLevels);
    for (UINT m = 0; m < in->MipLevels; ++m) {
        UINT w = std::max(1u, in->Width >> m), h = std::max(1u, in->Height >> m);
        mips[m].resize(size_t(w) * h * 2);
        for (UINT y = 0; y < h; ++y)
            memcpy(&mips[m][size_t(y) * w * 2], static_cast<const uint8_t*>(init[m].pSysMem) + size_t(y) * init[m].SysMemPitch,
                   size_t(w) * 4);
    }
    *out = *in;
    out->Usage = D3D11_USAGE_DEFAULT;
    out->CPUAccessFlags = 0;
    std::lock_guard lock(g_mutex);
    g_pending = std::move(mips);
    g_size = in->Width;
    return true;
}

void on_created(ID3D11Texture2D* tex) {
    std::lock_guard lock(g_mutex);
    if (g_pending.empty())
        return;
    if (g_tex)
        g_tex->Release();  // a newer copy (level reload): follow it
    g_tex = tex;
    g_tex->AddRef();
    g_original = std::move(g_pending);
    g_pending.clear();
    g_dirty = false;
    logf("shadow_noise: shadow rotation noise found (64x64, %zu mips)", g_original.size());
}

void set_active(bool on) { g_active = on; }
void set_coarseness(int k) { g_coarse = std::clamp(k, 1, 8); }

// k > 1: mip 0 (the only one the shadow PS reads: point sampler, max LOD 0) holds the original's top-left
// (64/k)^2 texels, each repeated over k x k: k times lower noise frequency.
static void upload(ID3D11DeviceContext* ctx, float angle, int k) {
    float c = cosf(angle), s = sinf(angle);
    std::vector<int16_t> buf;
    for (UINT m = 0; m < g_original.size(); ++m) {
        const std::vector<int16_t>& src = g_original[m];
        const UINT w = std::max(1u, g_size >> m);
        buf.resize(src.size());
        for (size_t i = 0; i < src.size(); i += 2) {
            size_t j = i;
            if (m == 0 && k > 1) {
                size_t px = (i / 2) % w, py = (i / 2) / w;
                j = ((py / k) * w + px / k) * 2;
            }
            float x = std::max(src[j] / 32767.0f, -1.0f), y = std::max(src[j + 1] / 32767.0f, -1.0f);
            float rx = x * c - y * s, ry = x * s + y * c;
            buf[i] = static_cast<int16_t>(lrintf(std::clamp(rx, -1.0f, 1.0f) * 32767.0f));
            buf[i + 1] = static_cast<int16_t>(lrintf(std::clamp(ry, -1.0f, 1.0f) * 32767.0f));
        }
        ctx->UpdateSubresource(g_tex, m, nullptr, buf.data(), w * 4, 0);
    }
}

void on_present(IDXGISwapChain* swapchain) {
    std::lock_guard lock(g_mutex);
    if (!g_tex || (!g_active && !g_dirty && g_coarse == g_uploaded_coarse))
        return;
    ID3D11Device* device = nullptr;
    if (FAILED(swapchain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&device))))
        return;
    ID3D11DeviceContext* ctx = nullptr;
    device->GetImmediateContext(&ctx);
    device->Release();
    if (g_active) {
        g_angle = fmodf(g_angle + 2.39996323f, 6.28318531f);  // golden angle: well spread over any frame window
        upload(ctx, g_angle, g_coarse);
        g_dirty = true;
    } else {
        upload(ctx, 0.0f, g_coarse);  // the engine's own pattern (coarsened if asked)
        g_dirty = false;
    }
    g_uploaded_coarse = g_coarse;
    ctx->Release();
}

}  // namespace kc::shadow_noise
