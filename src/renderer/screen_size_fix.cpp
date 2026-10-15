#include "renderer/screen_size_fix.h"

#include <d3dcompiler.h>

#include <cmath>
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
    if (contains(code, size, "SvoTreeSettings0") && contains(code, size, "g_mViewProj")) {
        ID3D11ShaderReflection* r = nullptr;
        if (FAILED(D3DReflect(code, size, __uuidof(ID3D11ShaderReflection), reinterpret_cast<void**>(&r))))
            return false;
        bool ok = find_var(r, "g_mViewProj", &out->cb_slot, &out->offset) && out->offset % 16 == 0;
        r->Release();
        if (!ok)
            return false;
        out->kind = Shader::ViewProjPS;
        ++g_status.ps_seen;
        logf("screen_size_fix: SVO trace PS: g_mViewProj cb%u +%u", out->cb_slot, out->offset);
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

static Patch g_ps_patch, g_vs_patch, g_bloom_patch;

// Texture the luminance pass read last (identity only): the quarter-res scene, also the bloom's source.
static void* g_lum_source;

static void* ps_resource(ID3D11DeviceContext* ctx, UINT slot) {
    ID3D11ShaderResourceView* srv = nullptr;
    ctx->PSGetShaderResources(slot, 1, &srv);
    if (!srv)
        return nullptr;
    ID3D11Resource* res = nullptr;
    srv->GetResource(&res);
    srv->Release();
    if (res)
        res->Release();
    return res;
}

// Size of render target 0 (or of the depth target when there is no colour target); false if none.
static bool target_size(ID3D11DeviceContext* ctx, UINT* w, UINT* h) {
    ID3D11RenderTargetView* rtv = nullptr;
    ID3D11DepthStencilView* dsv = nullptr;
    ctx->OMGetRenderTargets(1, &rtv, &dsv);
    ID3D11View* view = rtv ? static_cast<ID3D11View*>(rtv) : static_cast<ID3D11View*>(dsv);
    bool ok = false;
    if (view) {
        ID3D11Resource* res = nullptr;
        view->GetResource(&res);
        D3D11_RESOURCE_DIMENSION dim;
        res->GetType(&dim);
        if (dim == D3D11_RESOURCE_DIMENSION_TEXTURE2D) {
            D3D11_TEXTURE2D_DESC td;
            static_cast<ID3D11Texture2D*>(res)->GetDesc(&td);
            *w = td.Width;
            *h = td.Height;
            ok = true;
        }
        res->Release();
    }
    if (rtv)
        rtv->Release();
    if (dsv)
        dsv->Release();
    return ok;
}

TriAction scaled_tri_action(ID3D11DeviceContext* ctx, const Shader* vs) {
    if (!g_status.enabled || !game::inside_scaled_fullscreen_tri())
        return TriAction::Draw;
    if (vs) {
        ++g_status.vs_fixed;
        return TriAction::Redraw;
    }
    D3D11_VIEWPORT vp;
    UINT n = 1, w = 0, h = 0;
    ctx->RSGetViewports(&n, &vp);
    if (n == 0 || !target_size(ctx, &w, &h))
        return TriAction::Draw;
    bool covers = vp.TopLeftX == 0 && vp.TopLeftY == 0 && vp.Width >= float(w) - 0.5f && vp.Height >= float(h) - 0.5f;
    if (!covers)
        return TriAction::Draw;
    // Screen-proportional: same aspect as the native render size (426x240 vs 2560x1440).
    game::Vec2 native = game::native_size();
    float aspect = float(w) / float(h), screen = native.y > 0 ? native.x / native.y : 0;
    if (screen > 0 && fabsf(aspect / screen - 1.0f) < 0.01f) {
        ++g_status.shrunk;
        return TriAction::Shrink;
    }
    ++g_status.non_screen;
    return TriAction::Redraw;
}

// g_mViewProj patch, done on the GPU (the engine's constant buffer is GPU-only; reading it back would stall the
// frame): a compute shader reads the engine buffer as a constant buffer and writes it to a raw UAV buffer with the
// clip x/y rows remapped into the sub-rect (uv' = uv * s): row0' = s.x*row0 + (s.x-1)*row3,
// row1' = s.y*row1 + (1-s.y)*row3 (y is flipped in the UV). Then the UAV buffer is copied into a constant buffer.
static const char kViewProjCS[] = R"(
cbuffer Src : register(b0) { float4 src[4096]; };
cbuffer Params : register(b1) { uint reg; uint count; float sx; float sy; };
RWByteAddressBuffer dst : register(u0);
[numthreads(64, 1, 1)]
void main(uint i : SV_DispatchThreadID) {
    if (i >= count)
        return;
    float4 v = src[i];
    if (i == reg)
        v = sx * src[reg] + (sx - 1.0) * src[reg + 3];
    else if (i == reg + 1)
        v = sy * src[reg + 1] + (1.0 - sy) * src[reg + 3];
    dst.Store4(i * 16, asuint(v));
}
)";

struct ViewProjPatch {
    bool failed = false;
    ID3D11ComputeShader* cs = nullptr;
    ID3D11Buffer* params = nullptr;
    ID3D11Buffer* raw = nullptr;
    ID3D11UnorderedAccessView* uav = nullptr;
    ID3D11Buffer* copy = nullptr;
    UINT size = 0;

    bool ensure(ID3D11Device* device, UINT bytes) {
        if (failed)
            return false;
        if (!cs) {
            ID3DBlob* code = nullptr;
            ID3DBlob* errors = nullptr;
            if (FAILED(D3DCompile(kViewProjCS, sizeof(kViewProjCS) - 1, "viewproj_cs", nullptr, nullptr, "main",
                                  "cs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors))) {
                logf("screen_size_fix: viewproj CS compile failed: %s",
                     errors ? static_cast<const char*>(errors->GetBufferPointer()) : "");
                if (errors)
                    errors->Release();
                failed = true;
                return false;
            }
            HRESULT hr = device->CreateComputeShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &cs);
            code->Release();
            D3D11_BUFFER_DESC d = {16, D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0, 0};
            if (FAILED(hr) || FAILED(device->CreateBuffer(&d, nullptr, &params))) {
                failed = true;
                return false;
            }
        }
        if (size == bytes)
            return true;
        if (uav)
            uav->Release();
        if (raw)
            raw->Release();
        if (copy)
            copy->Release();
        uav = nullptr;
        raw = copy = nullptr;
        size = 0;
        D3D11_BUFFER_DESC rd = {bytes, D3D11_USAGE_DEFAULT, D3D11_BIND_UNORDERED_ACCESS, 0,
                                D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS, 0};
        D3D11_BUFFER_DESC cd = {bytes, D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0, 0};
        D3D11_UNORDERED_ACCESS_VIEW_DESC ud = {};
        ud.Format = DXGI_FORMAT_R32_TYPELESS;
        ud.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        ud.Buffer.NumElements = bytes / 4;
        ud.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
        if (FAILED(device->CreateBuffer(&rd, nullptr, &raw)) || FAILED(device->CreateUnorderedAccessView(raw, &ud, &uav)) ||
            FAILED(device->CreateBuffer(&cd, nullptr, &copy))) {
            logf("screen_size_fix: viewproj buffers (%u bytes) failed", bytes);
            failed = true;
            return false;
        }
        size = bytes;
        return true;
    }

    // Returns the patched buffer, or nullptr.
    ID3D11Buffer* make(ID3D11DeviceContext* ctx, ID3D11Buffer* src, UINT offset, float sx, float sy) {
        D3D11_BUFFER_DESC bd;
        src->GetDesc(&bd);
        if (offset + 64 > bd.ByteWidth || bd.ByteWidth % 16 != 0 || bd.ByteWidth > 4096 * 16)
            return nullptr;
        ID3D11Device* device = nullptr;
        ctx->GetDevice(&device);
        bool ok = ensure(device, bd.ByteWidth);
        device->Release();
        if (!ok)
            return nullptr;
        struct {
            UINT reg, count;
            float sx, sy;
        } p = {offset / 16, bd.ByteWidth / 16, sx, sy};
        ctx->UpdateSubresource(params, 0, nullptr, &p, 0, 0);

        // Save the compute bindings we touch.
        ID3D11ComputeShader* old_cs = nullptr;
        ID3D11ClassInstance* inst[256];
        UINT ninst = 256;
        ctx->CSGetShader(&old_cs, inst, &ninst);
        ID3D11Buffer* old_cb[2] = {};
        ctx->CSGetConstantBuffers(0, 2, old_cb);
        ID3D11UnorderedAccessView* old_uav = nullptr;
        ctx->CSGetUnorderedAccessViews(0, 1, &old_uav);

        ctx->CSSetShader(cs, nullptr, 0);
        ID3D11Buffer* cbs[2] = {src, params};
        ctx->CSSetConstantBuffers(0, 2, cbs);
        ctx->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
        ctx->Dispatch((p.count + 63) / 64, 1, 1);
        ID3D11UnorderedAccessView* null_uav = nullptr;
        ctx->CSSetUnorderedAccessViews(0, 1, &null_uav, nullptr);
        ctx->CopyResource(copy, raw);

        UINT keep = ~0u;
        ctx->CSSetUnorderedAccessViews(0, 1, &old_uav, &keep);
        ctx->CSSetConstantBuffers(0, 2, old_cb);
        ctx->CSSetShader(old_cs, inst, ninst);
        if (old_cs)
            old_cs->Release();
        for (UINT i = 0; i < ninst; ++i)
            if (inst[i])
                inst[i]->Release();
        for (ID3D11Buffer* b : old_cb)
            if (b)
                b->Release();
        if (old_uav)
            old_uav->Release();
        return copy;
    }
};

static ViewProjPatch g_viewproj_patch;

void draw(ID3D11DeviceContext* ctx, const Shader& ps, const Shader* vs, UINT count, UINT start, DrawFn original) {
    if (!g_status.enabled) {
        original(ctx, count, start);
        return;
    }
    if (ps.kind == Shader::ViewProjPS) {
        game::Vec2 d = game::current_downscale();
        ID3D11Buffer* cb = nullptr;
        if (d.x < 0.999f || d.y < 0.999f)
            ctx->PSGetConstantBuffers(ps.cb_slot, 1, &cb);
        ID3D11Buffer* b = cb ? g_viewproj_patch.make(ctx, cb, ps.offset, d.x, d.y) : nullptr;
        if (b) {
            ctx->PSSetConstantBuffers(ps.cb_slot, 1, &b);
            ++g_status.ps_fixed;
        }
        original(ctx, count, start);
        if (b)
            ctx->PSSetConstantBuffers(ps.cb_slot, 1, &cb);
        if (cb)
            cb->Release();
        return;
    }
    if (ps.kind == Shader::LumInitialPS) {
        g_lum_source = ps_resource(ctx, 0);
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

bool draw_bloom_source(ID3D11DeviceContext* ctx, const Shader& vs, UINT count, UINT start, DrawFn original) {
    if (!g_status.enabled || vs.kind != Shader::SelfScalingVS || !g_lum_source)
        return false;
    game::Vec2 d = game::current_downscale();
    if (!(d.x < 0.999f || d.y < 0.999f) || ps_resource(ctx, 0) != g_lum_source)
        return false;
    ID3D11Buffer* cb = nullptr;
    ctx->VSGetConstantBuffers(vs.cb_slot, 1, &cb);
    ID3D11Buffer* b = cb ? g_bloom_patch.make(ctx, cb, vs.offset, d.x, d.y) : nullptr;
    if (!b) {
        if (cb)
            cb->Release();
        return false;
    }
    ctx->VSSetConstantBuffers(vs.cb_slot, 1, &b);
    original(ctx, count, start);
    ctx->VSSetConstantBuffers(vs.cb_slot, 1, &cb);
    cb->Release();
    ++g_status.bloom_fixed;
    return true;
}

}  // namespace kc::screen_size_fix
