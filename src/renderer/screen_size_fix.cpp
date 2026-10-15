#include "renderer/screen_size_fix.h"

#include <d3dcompiler.h>

#include <cstring>

#include "core/log.h"
#include "game/renderer.h"

namespace kc::screen_size_fix {

static Status g_status;
static ID3D11Buffer* g_copy;   // same size as the engine's buffer, rebuilt when that changes
static UINT g_copy_size;
static ID3D11Buffer* g_value;  // 16 bytes: (full width, full height, 0, 0)

Status& status() { return g_status; }

static bool contains(const void* data, SIZE_T size, const char* s) {
    const size_t n = strlen(s);
    const auto* p = static_cast<const char*>(data);
    for (SIZE_T i = 0; i + n <= size; ++i)
        if (p[i] == s[0] && memcmp(p + i, s, n) == 0)
            return true;
    return false;
}

bool reflect(const void* code, SIZE_T size, Shader* out) {
    if (!contains(code, size, "zMap") || !contains(code, size, "PS_ScreenSize"))
        return false;
    ID3D11ShaderReflection* r = nullptr;
    if (FAILED(D3DReflect(code, size, __uuidof(ID3D11ShaderReflection), reinterpret_cast<void**>(&r))))
        return false;
    D3D11_SHADER_DESC sd;
    r->GetDesc(&sd);
    for (UINT i = 0; i < sd.BoundResources; ++i) {
        D3D11_SHADER_INPUT_BIND_DESC b;
        r->GetResourceBindingDesc(i, &b);
        if (b.Type == D3D_SIT_TEXTURE && b.Dimension == D3D_SRV_DIMENSION_TEXTURE2D && !strcmp(b.Name, "zMap"))
            out->zmap_slot = b.BindPoint;
    }
    for (UINT c = 0; c < sd.ConstantBuffers; ++c) {
        ID3D11ShaderReflectionConstantBuffer* cb = r->GetConstantBufferByIndex(c);
        D3D11_SHADER_BUFFER_DESC cbd;
        cb->GetDesc(&cbd);
        for (UINT v = 0; v < cbd.Variables; ++v) {
            D3D11_SHADER_VARIABLE_DESC vd;
            cb->GetVariableByIndex(v)->GetDesc(&vd);
            if (strcmp(vd.Name, "PS_ScreenSize") != 0 || !(vd.uFlags & D3D_SVF_USED))
                continue;
            D3D11_SHADER_INPUT_BIND_DESC b;
            if (SUCCEEDED(r->GetResourceBindingDescByName(cbd.Name, &b)))
                out->cb_slot = b.BindPoint;
            out->offset = vd.StartOffset;
        }
    }
    r->Release();
    if (out->zmap_slot == ~0u || out->cb_slot == ~0u)
        return false;
    ++g_status.shaders_seen;
    logf("screen_size_fix: shader with zMap t%u, PS_ScreenSize cb%u +%u", out->zmap_slot, out->cb_slot, out->offset);
    return true;
}

static bool ensure(ID3D11Device* device, UINT size) {
    if (!g_value) {
        D3D11_BUFFER_DESC d = {16, D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0, 0};
        if (FAILED(device->CreateBuffer(&d, nullptr, &g_value)))
            return false;
    }
    if (g_copy && g_copy_size == size)
        return true;
    if (g_copy)
        g_copy->Release();
    g_copy = nullptr;
    D3D11_BUFFER_DESC d = {size, D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0, 0};
    if (FAILED(device->CreateBuffer(&d, nullptr, &g_copy)))
        return false;
    g_copy_size = size;
    return true;
}

void draw(ID3D11DeviceContext* ctx, const Shader& s, UINT count, UINT start, DrawFn original) {
    game::Vec2 scale = game::current_viewport_scale();
    if (!g_status.enabled || (scale.x >= 0.999f && scale.y >= 0.999f)) {
        original(ctx, count, start);
        return;
    }
    ID3D11Buffer* cb = nullptr;
    ID3D11ShaderResourceView* zmap = nullptr;
    ctx->PSGetConstantBuffers(s.cb_slot, 1, &cb);
    ctx->PSGetShaderResources(s.zmap_slot, 1, &zmap);
    bool fixed = false;
    if (cb && zmap) {
        ID3D11Resource* res = nullptr;
        zmap->GetResource(&res);
        D3D11_TEXTURE2D_DESC td;
        static_cast<ID3D11Texture2D*>(res)->GetDesc(&td);
        res->Release();
        D3D11_BUFFER_DESC bd;
        cb->GetDesc(&bd);
        ID3D11Device* device = nullptr;
        ctx->GetDevice(&device);
        bool ok = s.offset + 8 <= bd.ByteWidth && ensure(device, bd.ByteWidth);
        device->Release();
        if (ok) {
            const float value[4] = {static_cast<float>(td.Width), static_cast<float>(td.Height), 0, 0};
            ctx->UpdateSubresource(g_value, 0, nullptr, value, 0, 0);
            ctx->CopyResource(g_copy, cb);
            D3D11_BOX box = {0, 0, 0, 8, 1, 1};
            ctx->CopySubresourceRegion(g_copy, 0, s.offset, 0, 0, g_value, 0, &box);
            ctx->PSSetConstantBuffers(s.cb_slot, 1, &g_copy);
            original(ctx, count, start);
            ctx->PSSetConstantBuffers(s.cb_slot, 1, &cb);
            ++g_status.draws_fixed;
            fixed = true;
        }
    }
    if (!fixed)
        original(ctx, count, start);
    if (cb)
        cb->Release();
    if (zmap)
        zmap->Release();
}

}  // namespace kc::screen_size_fix
