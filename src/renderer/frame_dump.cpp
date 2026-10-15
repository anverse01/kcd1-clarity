#include "renderer/frame_dump.h"

#include <windows.h>

#include <dxgi.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "core/log.h"
#include "core/state.h"

namespace kc::frame_dump {

enum class Phase { Idle, Requested, Saving };
static Phase g_phase = Phase::Idle;
static int g_index;
static int g_files;
static std::string g_result = "";

void request() { g_phase = Phase::Requested; }
bool armed() { return g_phase == Phase::Requested || g_phase == Phase::Saving; }
const std::string& last_result() { return g_result; }

static float half_to_float(uint16_t h) {
    uint32_t s = (h >> 15) & 1, e = (h >> 10) & 0x1f, m = h & 0x3ff;
    float v = e == 0 ? std::ldexp(float(m), -24) : e == 31 ? (m ? NAN : INFINITY) : std::ldexp(float(m | 0x400), int(e) - 25);
    return s ? -v : v;
}

static uint8_t srgb8(float c) {
    if (!(c > 0)) return 0;  // also NaN
    if (c >= 1) return 255;
    c = c < 0.0031308f ? 12.92f * c : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
    return static_cast<uint8_t>(c * 255.0f + 0.5f);
}

static uint8_t unorm8(float c) { return c > 0 ? (c >= 1 ? 255 : static_cast<uint8_t>(c * 255.0f + 0.5f)) : 0; }

// Converts one texel to BGR. Returns false for unsupported formats.
static bool texel(DXGI_FORMAT f, const uint8_t* p, uint8_t bgr[3]) {
    switch (f) {
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        bgr[0] = p[2], bgr[1] = p[1], bgr[2] = p[0];
        return true;
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        bgr[0] = p[0], bgr[1] = p[1], bgr[2] = p[2];
        return true;
    case DXGI_FORMAT_R16G16B16A16_FLOAT: {
        uint16_t h[3];
        memcpy(h, p, 6);
        bool nan = std::isnan(half_to_float(h[0])) || std::isnan(half_to_float(h[1])) || std::isnan(half_to_float(h[2]));
        if (nan) { bgr[0] = 255, bgr[1] = 0, bgr[2] = 255; return true; }
        bgr[0] = srgb8(half_to_float(h[2])), bgr[1] = srgb8(half_to_float(h[1])), bgr[2] = srgb8(half_to_float(h[0]));
        return true;
    }
    case DXGI_FORMAT_R16G16_FLOAT: {  // motion vectors in UV: 0.5 + mv * 50
        uint16_t h[2];
        memcpy(h, p, 4);
        bgr[0] = 0, bgr[1] = unorm8(0.5f + half_to_float(h[1]) * 50.0f), bgr[2] = unorm8(0.5f + half_to_float(h[0]) * 50.0f);
        return true;
    }
    case DXGI_FORMAT_R32_FLOAT: {  // depth (reversed Z: near = 1): sqrt for contrast
        float d;
        memcpy(&d, p, 4);
        bgr[0] = bgr[1] = bgr[2] = unorm8(std::sqrt(d < 0 ? 0 : d));
        return true;
    }
    default:
        return false;
    }
}

static UINT texel_size(DXGI_FORMAT f) {
    switch (f) {
    case DXGI_FORMAT_R16G16B16A16_FLOAT: return 8;
    default: return 4;
    }
}

void save(ID3D11DeviceContext* ctx, ID3D11Resource* res, const char* name) {
    if (!armed() || !res)
        return;
    if (g_phase == Phase::Requested) {
        g_phase = Phase::Saving;
        ++g_index;
        g_files = 0;
        CreateDirectoryW((state().mod_dir + L"kcd_dumps").c_str(), nullptr);
    }
    ID3D11Texture2D* tex = nullptr;
    if (FAILED(res->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&tex))))
        return;
    D3D11_TEXTURE2D_DESC d;
    tex->GetDesc(&d);
    tex->Release();
    D3D11_TEXTURE2D_DESC sd = d;
    sd.MipLevels = 1;
    sd.ArraySize = 1;
    sd.SampleDesc = {1, 0};
    sd.Usage = D3D11_USAGE_STAGING;
    sd.BindFlags = 0;
    sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    sd.MiscFlags = 0;
    ID3D11Device* device = nullptr;
    ctx->GetDevice(&device);
    ID3D11Texture2D* staging = nullptr;
    HRESULT hr = device->CreateTexture2D(&sd, nullptr, &staging);
    device->Release();
    if (FAILED(hr)) {
        logf("frame_dump: %s: staging texture failed 0x%08lx (format %d)", name, hr, d.Format);
        return;
    }
    ctx->CopySubresourceRegion(staging, 0, 0, 0, 0, res, 0, nullptr);
    D3D11_MAPPED_SUBRESOURCE m;
    if (FAILED(ctx->Map(staging, 0, D3D11_MAP_READ, 0, &m))) {
        staging->Release();
        return;
    }
    const UINT w = d.Width, h = d.Height, row = (w * 3 + 3) & ~3u;
    std::vector<uint8_t> bmp(54 + size_t(row) * h);
    uint8_t* hd = bmp.data();
    auto put32 = [&](size_t at, uint32_t v) { memcpy(hd + at, &v, 4); };
    hd[0] = 'B', hd[1] = 'M';
    put32(2, uint32_t(bmp.size()));
    put32(10, 54);
    put32(14, 40);
    put32(18, w);
    put32(22, h);
    hd[26] = 1, hd[28] = 24;
    put32(34, uint32_t(row) * h);
    bool ok = true;
    for (UINT y = 0; y < h && ok; ++y) {
        const uint8_t* src = static_cast<const uint8_t*>(m.pData) + size_t(y) * m.RowPitch;
        uint8_t* dst = bmp.data() + 54 + size_t(h - 1 - y) * row;
        for (UINT x = 0; x < w && ok; ++x)
            ok = texel(d.Format, src + size_t(x) * texel_size(d.Format), dst + x * 3);
    }
    ctx->Unmap(staging, 0);
    staging->Release();
    if (!ok) {
        logf("frame_dump: %s: unsupported format %d", name, d.Format);
        return;
    }
    char file[64];
    snprintf(file, sizeof(file), "%03d_%s.bmp", g_index, name);
    std::wstring path = state().mod_dir + L"kcd_dumps\\" + std::wstring(file, file + strlen(file));
    FILE* f = nullptr;
    _wfopen_s(&f, path.c_str(), L"wb");
    if (f) {
        fwrite(bmp.data(), 1, bmp.size(), f);
        fclose(f);
        ++g_files;
    }
    logf("frame_dump: %s %ux%u format %d -> %s", name, w, h, d.Format, f ? file : "WRITE FAILED");
}

void on_present(IDXGISwapChain* swapchain) {
    if (g_phase != Phase::Saving)
        return;
    ID3D11Texture2D* back = nullptr;
    if (SUCCEEDED(swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back)))) {
        ID3D11Device* device = nullptr;
        back->GetDevice(&device);
        ID3D11DeviceContext* ctx = nullptr;
        device->GetImmediateContext(&ctx);
        save(ctx, back, "backbuffer");
        ctx->Release();
        device->Release();
        back->Release();
    }
    g_phase = Phase::Idle;
    char buf[96];
    snprintf(buf, sizeof(buf), "dump %03d: %d files in Bin\\Win64\\kcd_dumps", g_index, g_files);
    g_result = buf;
    logf("frame_dump: %s", buf);
}

}  // namespace kc::frame_dump
