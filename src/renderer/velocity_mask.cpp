#include "renderer/velocity_mask.h"

#include <d3dcompiler.h>

#include "core/log.h"
#include "renderer/state_backup.h"

namespace kc::velocity_mask {

static Status g_status;
static bool g_prev_velocity;     // the last binding had the velocity target (with a depth target)
static void* g_prev_depth;       // identity only
static ID3D11Resource* g_prev_velocity_res;  // AddRef'd: the velocity target of the last binding
static bool g_valid;             // copy taken this frame
static ID3D11Texture2D* g_copy;
static ID3D11ShaderResourceView* g_srv;
static D3D11_TEXTURE2D_DESC g_copy_desc;
static bool g_busy;              // our own pass: ignore the bindings it makes
static bool g_pending;           // copy taken, cleanup waits for the G-buffer to finish
static void* g_static_rt0;       // identity: the static G-buffer binding's first target
static ID3D11Resource* g_velocity;  // AddRef'd while pending
static ID3D11Resource* g_depth;     // AddRef'd while pending

// Cleanup pass: zero (= "no object motion") the velocity wherever the final depth differs from the copy.
static const char kVS[] = R"(
float4 main(uint id : SV_VertexID) : SV_Position {
    float2 uv = float2((id << 1) & 2, id & 2);
    return float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
}
)";
static const char kPS[] = R"(
Texture2D<float> Copy : register(t0);
Texture2D<float> Depth : register(t1);
float4 main(float4 pos : SV_Position) : SV_Target {
    int3 p = int3(pos.xy, 0);
    if (Copy.Load(p) == Depth.Load(p))
        discard;
    return 0;
}
)";
static ID3D11VertexShader* g_vs;
static ID3D11PixelShader* g_ps;
static bool g_shaders_failed;
// Views, kept while their resource is the same one (AddRef'd resource as the key).
static ID3D11Resource* g_depth_key;
static ID3D11ShaderResourceView* g_depth_srv;
static ID3D11Resource* g_vel_key;
static ID3D11RenderTargetView* g_vel_rtv;

Status& status() { return g_status; }

static ID3D11Resource* resource(ID3D11View* v) {
    ID3D11Resource* r = nullptr;
    if (v)
        v->GetResource(&r);
    return r;  // AddRef'd
}

// The velocity target: the only R8G8_SNORM render target (R16G16_SNORM when velocity_format promoted it).
static bool is_velocity(ID3D11RenderTargetView* rtv) {
    ID3D11Resource* r = resource(rtv);
    if (!r)
        return false;
    D3D11_RESOURCE_DIMENSION dim;
    r->GetType(&dim);
    bool yes = false;
    if (dim == D3D11_RESOURCE_DIMENSION_TEXTURE2D) {
        D3D11_TEXTURE2D_DESC d;
        static_cast<ID3D11Texture2D*>(r)->GetDesc(&d);
        yes = (d.Format == DXGI_FORMAT_R8G8_SNORM || d.Format == DXGI_FORMAT_R16G16_SNORM) && d.Width >= 640 &&
              d.Height >= 360 && d.SampleDesc.Count == 1;
    }
    r->Release();
    return yes;
}

static DXGI_FORMAT depth_view_format(DXGI_FORMAT f) {
    switch (f) {
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT: return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT: return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT: return DXGI_FORMAT_R32_FLOAT;
    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_D16_UNORM: return DXGI_FORMAT_R16_UNORM;
    default: return DXGI_FORMAT_UNKNOWN;
    }
}

static void snapshot(ID3D11DeviceContext* ctx, ID3D11Resource* depth) {
    D3D11_RESOURCE_DIMENSION dim;
    depth->GetType(&dim);
    if (dim != D3D11_RESOURCE_DIMENSION_TEXTURE2D)
        return;
    D3D11_TEXTURE2D_DESC d;
    static_cast<ID3D11Texture2D*>(depth)->GetDesc(&d);
    const DXGI_FORMAT view = depth_view_format(d.Format);
    if (view == DXGI_FORMAT_UNKNOWN || d.SampleDesc.Count != 1 || d.ArraySize != 1)
        return;
    if (!g_copy || g_copy_desc.Width != d.Width || g_copy_desc.Height != d.Height || g_copy_desc.Format != d.Format ||
        g_copy_desc.MipLevels != d.MipLevels) {
        if (g_srv)
            g_srv->Release(), g_srv = nullptr;
        if (g_copy)
            g_copy->Release(), g_copy = nullptr;
        D3D11_TEXTURE2D_DESC c = d;
        c.Usage = D3D11_USAGE_DEFAULT;
        c.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        c.CPUAccessFlags = 0;
        c.MiscFlags = 0;
        D3D11_SHADER_RESOURCE_VIEW_DESC sd = {};
        sd.Format = view;
        sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        sd.Texture2D.MipLevels = 1;
        ID3D11Device* device = nullptr;
        ctx->GetDevice(&device);
        bool ok = SUCCEEDED(device->CreateTexture2D(&c, nullptr, &g_copy)) &&
                  SUCCEEDED(device->CreateShaderResourceView(g_copy, &sd, &g_srv));
        device->Release();
        logf("velocity_mask: depth copy %ux%u format %d %s", d.Width, d.Height, d.Format, ok ? "ready" : "FAILED");
        if (!ok) {
            if (g_copy)
                g_copy->Release(), g_copy = nullptr;
            g_status.enabled = false;
            return;
        }
        g_copy_desc = d;
    }
    ctx->CopyResource(g_copy, depth);
    g_valid = true;
    ++g_status.snapshots;
}

static bool ensure_shaders(ID3D11Device* device) {
    if (g_ps || g_shaders_failed)
        return g_ps != nullptr;
    ID3DBlob* vc = nullptr;
    ID3DBlob* pc = nullptr;
    ID3DBlob* err = nullptr;
    bool ok = SUCCEEDED(D3DCompile(kVS, sizeof(kVS) - 1, "velmask_vs", nullptr, nullptr, "main", "vs_5_0",
                                   D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &vc, &err));
    if (err)
        logf("velocity_mask: VS: %s", static_cast<const char*>(err->GetBufferPointer())), err->Release(), err = nullptr;
    ok = ok && SUCCEEDED(D3DCompile(kPS, sizeof(kPS) - 1, "velmask_ps", nullptr, nullptr, "main", "ps_5_0",
                                    D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &pc, &err));
    if (err)
        logf("velocity_mask: PS: %s", static_cast<const char*>(err->GetBufferPointer())), err->Release();
    ok = ok && SUCCEEDED(device->CreateVertexShader(vc->GetBufferPointer(), vc->GetBufferSize(), nullptr, &g_vs)) &&
         SUCCEEDED(device->CreatePixelShader(pc->GetBufferPointer(), pc->GetBufferSize(), nullptr, &g_ps));
    if (vc)
        vc->Release();
    if (pc)
        pc->Release();
    if (!ok) {
        logf("velocity_mask: cleanup shaders failed");
        g_shaders_failed = true;
        if (g_ps)
            g_ps->Release(), g_ps = nullptr;
    }
    return ok;
}

template <typename V>
static void drop_view(ID3D11Resource*& key, V*& view) {
    if (view)
        view->Release(), view = nullptr;
    if (key)
        key->Release(), key = nullptr;
}

static void clear_pending() {
    if (g_velocity)
        g_velocity->Release(), g_velocity = nullptr;
    if (g_depth)
        g_depth->Release(), g_depth = nullptr;
    g_pending = false;
    g_static_rt0 = nullptr;
}

// The static G-buffer pass is over: fix the engine's velocity target itself, so every reader (TAA/DLSS, the SVO GI
// temporal passes) gets the camera motion on occluders.
static void cleanup(ID3D11DeviceContext* ctx, DrawFn draw) {
    ID3D11Device* device = nullptr;
    ctx->GetDevice(&device);
    bool ok = ensure_shaders(device);
    if (ok && g_depth_key != g_depth) {
        drop_view(g_depth_key, g_depth_srv);
        D3D11_SHADER_RESOURCE_VIEW_DESC sd = {};
        sd.Format = depth_view_format(g_copy_desc.Format);
        sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        sd.Texture2D.MipLevels = 1;
        if (SUCCEEDED(device->CreateShaderResourceView(g_depth, &sd, &g_depth_srv)))
            g_depth_key = g_depth, g_depth->AddRef();
    }
    if (ok && g_vel_key != g_velocity) {
        drop_view(g_vel_key, g_vel_rtv);
        if (SUCCEEDED(device->CreateRenderTargetView(g_velocity, nullptr, &g_vel_rtv)))
            g_vel_key = g_velocity, g_velocity->AddRef();
    }
    device->Release();
    if (!ok || !g_depth_srv || !g_vel_rtv) {
        if (ok)
            logf("velocity_mask: cleanup views failed, prepare-pass mask only");
        return;
    }
    StateBackup backup(ctx);
    ID3D11ShaderResourceView* null_srv[2] = {};
    ctx->PSSetShaderResources(0, 2, null_srv);
    ctx->OMSetRenderTargets(1, &g_vel_rtv, nullptr);
    ctx->OMSetBlendState(nullptr, nullptr, 0xffffffff);
    ctx->OMSetDepthStencilState(nullptr, 0);
    ctx->RSSetState(nullptr);
    D3D11_VIEWPORT vp = {0, 0, float(g_copy_desc.Width), float(g_copy_desc.Height), 0, 1};
    ctx->RSSetViewports(1, &vp);
    ctx->IASetInputLayout(nullptr);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(g_vs, nullptr, 0);
    ctx->GSSetShader(nullptr, nullptr, 0);
    ctx->HSSetShader(nullptr, nullptr, 0);
    ctx->DSSetShader(nullptr, nullptr, 0);
    ctx->PSSetShader(g_ps, nullptr, 0);
    ID3D11ShaderResourceView* srvs[2] = {g_srv, g_depth_srv};
    ctx->PSSetShaderResources(0, 2, srvs);
    draw(ctx, 3, 0);
    ++g_status.cleanups;
}

void on_set_targets(ID3D11DeviceContext* ctx, UINT n, ID3D11RenderTargetView* const* rtvs, ID3D11DepthStencilView* dsv,
                    DrawFn draw) {
    if (g_busy)
        return;
    if (!g_status.enabled) {
        g_valid = false;
        clear_pending();
        return;
    }
    g_busy = true;
    ID3D11Resource* rt0 = n > 0 && rtvs ? resource(rtvs[0]) : nullptr;
    if (g_pending && rt0 != g_static_rt0) {  // first binding after the static G-buffer pass
        cleanup(ctx, draw);
        clear_pending();
    }
    ID3D11RenderTargetView* velocity = nullptr;
    if (dsv && rtvs && n >= 2)
        for (UINT i = 0; i < n && !velocity; ++i)
            if (rtvs[i] && is_velocity(rtvs[i]))
                velocity = rtvs[i];
    ID3D11Resource* depth = resource(dsv);
    if (g_prev_velocity && !velocity && depth && depth == g_prev_depth) {
        snapshot(ctx, depth);  // the moving objects are done: their depth, before anything else covers them
        if (g_valid && g_prev_velocity_res && rt0) {
            clear_pending();
            g_pending = true;
            g_static_rt0 = rt0;
            g_velocity = g_prev_velocity_res;
            g_velocity->AddRef();
            g_depth = depth;
            g_depth->AddRef();
        }
    }
    if (g_prev_velocity_res)
        g_prev_velocity_res->Release(), g_prev_velocity_res = nullptr;
    if (velocity)
        g_prev_velocity_res = resource(velocity);
    g_prev_velocity = velocity != nullptr;
    g_prev_depth = depth;
    if (depth)
        depth->Release();
    if (rt0)
        rt0->Release();
    g_busy = false;
}

void on_present() {
    clear_pending();
    g_valid = false;
    g_prev_velocity = false;
    g_prev_depth = nullptr;
}

ID3D11ShaderResourceView* depth_copy() { return g_status.enabled && g_valid ? g_srv : nullptr; }

}  // namespace kc::velocity_mask
