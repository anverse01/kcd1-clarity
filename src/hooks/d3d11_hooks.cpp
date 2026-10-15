#include "hooks/d3d11_hooks.h"

#include <windows.h>

#include <d3dcompiler.h>

#include <MinHook.h>

#include <cstring>
#include <mutex>
#include <vector>

#include "core/log.h"
#include "renderer/taa_dlss.h"

namespace kc::d3d11 {

using CreatePixelShaderFn = HRESULT(STDMETHODCALLTYPE*)(ID3D11Device*, const void*, SIZE_T, ID3D11ClassLinkage*,
                                                         ID3D11PixelShader**);
using PSSetShaderFn = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, ID3D11PixelShader*, ID3D11ClassInstance* const*,
                                               UINT);
using DrawFn = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT);

// Vtable slots (d3d11.h declaration order).
constexpr int kCreatePixelShaderSlot = 15;  // ID3D11Device
constexpr int kPSSetShaderSlot = 9;         // ID3D11DeviceContext
constexpr int kDrawSlot = 13;

static CreatePixelShaderFn o_create_pixel_shader;
static PSSetShaderFn o_ps_set_shader;
static DrawFn o_draw;

static Status g_status;
static std::mutex g_mutex;  // guards g_taa (written on shader-creating threads, read on the render thread)
static std::vector<TaaShader> g_taa;
static ID3D11PixelShader* g_bound_ps;      // render thread
static const TaaShader* g_bound_taa;       // render thread: g_bound_ps if it is a TAA shader
static const TaaShader* g_last_taa;
static thread_local bool t_inside;         // our own draws do not re-enter

const Status& status() { return g_status; }
const TaaShader* last_taa_shader() { return g_last_taa; }

// Bytecode contains the string: cheap filter before reflecting.
static bool contains(const void* data, SIZE_T size, const char* s) {
    const size_t n = strlen(s);
    const auto* p = static_cast<const char*>(data);
    for (SIZE_T i = 0; i + n <= size; ++i)
        if (p[i] == s[0] && memcmp(p + i, s, n) == 0)
            return true;
    return false;
}

static bool reflect_taa(const void* code, SIZE_T size, TaaShader* out) {
    if (!contains(code, size, "mReprojection") || !contains(code, size, "PostAA_DepthTex"))
        return false;
    ID3D11ShaderReflection* r = nullptr;
    if (FAILED(D3DReflect(code, size, __uuidof(ID3D11ShaderReflection), reinterpret_cast<void**>(&r))))
        return false;
    D3D11_SHADER_DESC sd;
    r->GetDesc(&sd);
    for (UINT i = 0; i < sd.BoundResources; ++i) {
        D3D11_SHADER_INPUT_BIND_DESC b;
        r->GetResourceBindingDesc(i, &b);
        if (b.Type == D3D_SIT_TEXTURE) {
            if (!strcmp(b.Name, "_tex3"))
                out->slot_velocity = b.BindPoint;
            else if (!strcmp(b.Name, "_tex4"))
                out->slot_colour = b.BindPoint;
            else if (!strcmp(b.Name, "_tex5"))
                out->slot_history = b.BindPoint;
            else if (!strcmp(b.Name, "PostAA_DepthTex"))
                out->slot_depth = b.BindPoint;
        }
    }
    for (UINT c = 0; c < sd.ConstantBuffers; ++c) {
        ID3D11ShaderReflectionConstantBuffer* cb = r->GetConstantBufferByIndex(c);
        D3D11_SHADER_BUFFER_DESC cbd;
        cb->GetDesc(&cbd);
        for (UINT v = 0; v < cbd.Variables; ++v) {
            ID3D11ShaderReflectionVariable* var = cb->GetVariableByIndex(v);
            D3D11_SHADER_VARIABLE_DESC vd;
            var->GetDesc(&vd);
            if (strcmp(vd.Name, "mReprojection") != 0)
                continue;
            D3D11_SHADER_TYPE_DESC td;
            var->GetType()->GetDesc(&td);
            D3D11_SHADER_INPUT_BIND_DESC b;
            if (SUCCEEDED(r->GetResourceBindingDescByName(cbd.Name, &b)))
                out->cb_slot = b.BindPoint;
            out->reproj_offset = vd.StartOffset;
            out->reproj_row_major = td.Class == D3D_SVC_MATRIX_ROWS;
        }
    }
    r->Release();
    bool ok = out->slot_velocity != ~0u && out->slot_colour != ~0u && out->slot_depth != ~0u && out->cb_slot != ~0u;
    logf("d3d11: TAA shader: velocity t%u colour t%u history t%u depth t%u, mReprojection cb%u +%u (%s)%s",
         out->slot_velocity, out->slot_colour, out->slot_history, out->slot_depth, out->cb_slot, out->reproj_offset,
         out->reproj_row_major ? "row major" : "column major", ok ? "" : " INCOMPLETE, ignored");
    return ok;
}

static HRESULT STDMETHODCALLTYPE hk_create_pixel_shader(ID3D11Device* device, const void* code, SIZE_T size,
                                                        ID3D11ClassLinkage* linkage, ID3D11PixelShader** out) {
    HRESULT hr = o_create_pixel_shader(device, code, size, linkage, out);
    if (SUCCEEDED(hr) && out && *out && !t_inside) {
        TaaShader t;
        if (reflect_taa(code, size, &t)) {
            t.shader = *out;
            std::lock_guard lock(g_mutex);
            g_taa.reserve(64);  // pointers into g_taa are handed out; never reallocate below 64 shaders
            if (g_taa.size() < g_taa.capacity()) {
                g_taa.push_back(t);
                ++g_status.taa_shaders_seen;
            }
        }
    }
    return hr;
}

static void STDMETHODCALLTYPE hk_ps_set_shader(ID3D11DeviceContext* ctx, ID3D11PixelShader* ps,
                                               ID3D11ClassInstance* const* inst, UINT n) {
    if (ps != g_bound_ps && !t_inside) {
        g_bound_ps = ps;
        g_bound_taa = nullptr;
        if (ps) {
            std::lock_guard lock(g_mutex);
            for (const TaaShader& t : g_taa)
                if (t.shader == ps) {
                    g_bound_taa = &t;
                    break;
                }
        }
    }
    o_ps_set_shader(ctx, ps, inst, n);
}

static void STDMETHODCALLTYPE hk_draw(ID3D11DeviceContext* ctx, UINT count, UINT start) {
    if (g_bound_taa && !t_inside) {
        ++g_status.taa_draws;
        g_last_taa = g_bound_taa;
        t_inside = true;
        bool replaced = taa_dlss::on_taa_draw(ctx, *g_bound_taa);
        t_inside = false;
        if (replaced) {
            ++g_status.taa_replaced;
            return;
        }
    }
    o_draw(ctx, count, start);
}

ScopedInside::ScopedInside() { t_inside = true; }
ScopedInside::~ScopedInside() { t_inside = false; }

bool install() {
    auto create = reinterpret_cast<PFN_D3D11_CREATE_DEVICE>(
        GetProcAddress(GetModuleHandleW(L"d3d11.dll"), "D3D11CreateDevice"));
    if (!create)
        return false;
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* ctx = nullptr;
    // Same flags as the game (0), so the implementation behind the vtables is the one the game gets.
    HRESULT hr = create(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &device,
                        nullptr, &ctx);
    if (FAILED(hr)) {
        logf("d3d11: dummy device failed 0x%08lx", hr);
        return false;
    }
    void** dvt = *reinterpret_cast<void***>(device);
    void** cvt = *reinterpret_cast<void***>(ctx);
    MH_STATUS s1 = MH_CreateHook(dvt[kCreatePixelShaderSlot], &hk_create_pixel_shader,
                                 reinterpret_cast<void**>(&o_create_pixel_shader));
    MH_STATUS s2 = MH_CreateHook(cvt[kPSSetShaderSlot], &hk_ps_set_shader, reinterpret_cast<void**>(&o_ps_set_shader));
    MH_STATUS s3 = MH_CreateHook(cvt[kDrawSlot], &hk_draw, reinterpret_cast<void**>(&o_draw));
    MH_STATUS s4 = MH_EnableHook(MH_ALL_HOOKS);
    ctx->Release();
    device->Release();
    logf("d3d11: hooks CreatePixelShader %s, PSSetShader %s, Draw %s, enable %s", MH_StatusToString(s1),
         MH_StatusToString(s2), MH_StatusToString(s3), MH_StatusToString(s4));
    g_status.installed = s1 == MH_OK && s2 == MH_OK && s3 == MH_OK && s4 == MH_OK;
    return g_status.installed;
}

}  // namespace kc::d3d11
