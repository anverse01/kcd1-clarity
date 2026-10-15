#include "renderer/screen_size_fix.h"

#include <d3dcompiler.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "core/log.h"
#include "game/renderer.h"

namespace kc::screen_size_fix {

static Status g_status;

Status& status() { return g_status; }

static bool contains(const void* data, SIZE_T size, const char* s) {
    const size_t n = strlen(s);
    const auto* p = static_cast<const char*>(data);
    for (SIZE_T i = 0; i + n <= size; ++i)
        if (p[i] == s[0] && memcmp(p + i, s, n) == 0)
            return true;
    return false;
}

// Slot and offset of a used constant-buffer variable.
static bool find_var(ID3D11ShaderReflection* r, const char* name, UINT* slot, UINT* offset) {
    D3D11_SHADER_DESC sd;
    r->GetDesc(&sd);
    for (UINT c = 0; c < sd.ConstantBuffers; ++c) {
        ID3D11ShaderReflectionConstantBuffer* cb = r->GetConstantBufferByIndex(c);
        D3D11_SHADER_BUFFER_DESC cbd;
        cb->GetDesc(&cbd);
        for (UINT v = 0; v < cbd.Variables; ++v) {
            D3D11_SHADER_VARIABLE_DESC vd;
            cb->GetVariableByIndex(v)->GetDesc(&vd);
            if (strcmp(vd.Name, name) != 0 || !(vd.uFlags & D3D_SVF_USED))
                continue;
            D3D11_SHADER_INPUT_BIND_DESC b;
            if (FAILED(r->GetResourceBindingDescByName(cbd.Name, &b)))
                return false;
            *slot = b.BindPoint;
            *offset = vd.StartOffset;
            return true;
        }
    }
    return false;
}

bool reflect_ps(const void* code, SIZE_T size, Shader* out) {
    if (contains(code, size, "SampleLumOffsets0")) {
        out->kind = Shader::LumInitialPS;
        ++g_status.ps_seen;
        logf("screen_size_fix: luminance PS (HDRSampleLumInitial)");
        return true;
    }
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
    bool ok = find_var(r, "PS_ScreenSize", &out->cb_slot, &out->offset) && out->zmap_slot != ~0u;
    r->Release();
    if (!ok)
        return false;
    out->kind = Shader::FogPS;
    ++g_status.ps_seen;
    logf("screen_size_fix: fog PS: zMap t%u, PS_ScreenSize cb%u +%u", out->zmap_slot, out->cb_slot, out->offset);
    return true;
}

bool reflect_vs(const void* code, SIZE_T size, Shader* out) {
    if (!contains(code, size, "g_VS_HPosScale"))
        return false;
    ID3D11ShaderReflection* r = nullptr;
    if (FAILED(D3DReflect(code, size, __uuidof(ID3D11ShaderReflection), reinterpret_cast<void**>(&r))))
        return false;
    // Input register of TEXCOORD0 (the CPU triangle's UVs).
    D3D11_SHADER_DESC sd;
    r->GetDesc(&sd);
    int tc_reg = -1;
    for (UINT i = 0; i < sd.InputParameters; ++i) {
        D3D11_SIGNATURE_PARAMETER_DESC p;
        r->GetInputParameterDesc(i, &p);
        if (!_stricmp(p.SemanticName, "TEXCOORD") && p.SemanticIndex == 0)
            tc_reg = static_cast<int>(p.Register);
    }
    bool found = tc_reg >= 0 && find_var(r, "g_VS_HPosScale", &out->cb_slot, &out->offset);
    r->Release();
    if (!found)
        return false;
    // Self-scaling = some instruction multiplies the input UV by g_VS_HPosScale.xy.
    ID3DBlob* dis = nullptr;
    if (FAILED(D3DDisassemble(code, size, 0, nullptr, &dis)))
        return false;
    std::string text(static_cast<const char*>(dis->GetBufferPointer()), dis->GetBufferSize());
    dis->Release();
    char v[16], c[24];
    snprintf(v, sizeof(v), "v%d.xy", tc_reg);
    snprintf(c, sizeof(c), "cb%u[%u].xy", out->cb_slot, out->offset / 16);
    bool self_scaling = false;
    for (size_t pos = 0; pos < text.size();) {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos)
            end = text.size();
        std::string line = text.substr(pos, end - pos);
        size_t first = line.find_first_not_of(' ');
        if (first != std::string::npos && (line.compare(first, 4, "mul ") == 0 || line.compare(first, 4, "mad ") == 0) && line.find(v) != std::string::npos &&
            line.find(c) != std::string::npos)
            self_scaling = true;
        pos = end + 1;
    }
    if (!self_scaling || out->offset % 16 != 0)  // .xy of a float4 register
        return false;
    out->kind = Shader::SelfScalingVS;
    ++g_status.vs_seen;
    logf("screen_size_fix: self-scaling VS: TEXCOORD0 v%d * g_VS_HPosScale cb%u +%u", tc_reg, out->cb_slot,
         out->offset);
    return true;
}

// One patched constant buffer: a copy of the engine's with 8 bytes replaced.
struct Patch {
    ID3D11Buffer* copy = nullptr;
    UINT copy_size = 0;
    ID3D11Buffer* value = nullptr;  // 16 bytes

    bool ensure(ID3D11Device* device, UINT size) {
        if (!value) {
            D3D11_BUFFER_DESC d = {16, D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0, 0};
            if (FAILED(device->CreateBuffer(&d, nullptr, &value)))
                return false;
        }
        if (copy && copy_size == size)
            return true;
        if (copy)
            copy->Release();
        copy = nullptr;
        D3D11_BUFFER_DESC d = {size, D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0, 0};
        if (FAILED(device->CreateBuffer(&d, nullptr, &copy)))
            return false;
        copy_size = size;
        return true;
    }

    // Returns the patched buffer, or nullptr (engine buffer left alone).
    ID3D11Buffer* make(ID3D11DeviceContext* ctx, ID3D11Buffer* src, UINT offset, float x, float y) {
        D3D11_BUFFER_DESC bd;
        src->GetDesc(&bd);
        if (offset + 8 > bd.ByteWidth)
            return nullptr;
        ID3D11Device* device = nullptr;
        ctx->GetDevice(&device);
        bool ok = ensure(device, bd.ByteWidth);
        device->Release();
        if (!ok)
            return nullptr;
        const float v[4] = {x, y, 0, 0};
        ctx->UpdateSubresource(value, 0, nullptr, v, 0, 0);
        ctx->CopyResource(copy, src);
        D3D11_BOX box = {0, 0, 0, 8, 1, 1};
        ctx->CopySubresourceRegion(copy, 0, offset, 0, 0, value, 0, &box);
        return copy;
    }
};

static Patch g_ps_patch, g_vs_patch;

bool skip_for_vs(const Shader* vs) {
    if (!vs || !g_status.enabled || !game::inside_scaled_fullscreen_tri())
        return false;
    game::redo_fullscreen_tri_unscaled();
    ++g_status.vs_fixed;
    return true;
}

void draw(ID3D11DeviceContext* ctx, const Shader& ps, const Shader* vs, UINT count, UINT start, DrawFn original) {
    if (!g_status.enabled) {
        original(ctx, count, start);
        return;
    }
    if (ps.kind == Shader::LumInitialPS) {
        game::Vec2 d = game::current_downscale();
        ID3D11Buffer* cb = nullptr;
        if (vs && (d.x < 0.999f || d.y < 0.999f))
            ctx->VSGetConstantBuffers(vs->cb_slot, 1, &cb);
        ID3D11Buffer* b = cb ? g_vs_patch.make(ctx, cb, vs->offset, d.x, d.y) : nullptr;
        if (b) {
            ctx->VSSetConstantBuffers(vs->cb_slot, 1, &b);
            ++g_status.ps_fixed;
        }
        original(ctx, count, start);
        if (b)
            ctx->VSSetConstantBuffers(vs->cb_slot, 1, &cb);
        if (cb)
            cb->Release();
        return;
    }
    game::Vec2 scale = game::current_viewport_scale();
    if (scale.x >= 0.999f && scale.y >= 0.999f) {
        original(ctx, count, start);
        return;
    }
    ID3D11Buffer* cb = nullptr;
    ID3D11ShaderResourceView* zmap = nullptr;
    ctx->PSGetShaderResources(ps.zmap_slot, 1, &zmap);
    ctx->PSGetConstantBuffers(ps.cb_slot, 1, &cb);
    bool patched = false;
    if (zmap && cb) {
        ID3D11Resource* res = nullptr;
        zmap->GetResource(&res);
        D3D11_TEXTURE2D_DESC td;
        static_cast<ID3D11Texture2D*>(res)->GetDesc(&td);
        res->Release();
        if (ID3D11Buffer* b = g_ps_patch.make(ctx, cb, ps.offset, float(td.Width), float(td.Height))) {
            ctx->PSSetConstantBuffers(ps.cb_slot, 1, &b);
            ++g_status.ps_fixed;
            patched = true;
        }
    }
    original(ctx, count, start);
    if (patched)
        ctx->PSSetConstantBuffers(ps.cb_slot, 1, &cb);
    if (cb)
        cb->Release();
    if (zmap)
        zmap->Release();
}

}  // namespace kc::screen_size_fix
