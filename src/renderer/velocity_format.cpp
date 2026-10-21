#include "renderer/velocity_format.h"

#include <atomic>

#include "core/log.h"

namespace kc::velocity_format {

static std::atomic<bool> g_enabled{true};
static std::atomic<unsigned> g_promoted{0};

void set_enabled(bool on) { g_enabled = on; }
unsigned promoted() { return g_promoted; }

// The only R8G8_SNORM render target the engine makes (kcd_frame8668: 119622, screen size, 1 mip).
static bool is_velocity_target(const D3D11_TEXTURE2D_DESC& d) {
    return d.Format == DXGI_FORMAT_R8G8_SNORM && (d.BindFlags & D3D11_BIND_RENDER_TARGET) && d.MipLevels == 1 &&
           d.ArraySize == 1 && d.SampleDesc.Count == 1 && d.Width >= 640 && d.Height >= 360;
}

bool adjust_texture(const D3D11_TEXTURE2D_DESC* in, D3D11_TEXTURE2D_DESC* out) {
    if (!g_enabled || !in || !is_velocity_target(*in))
        return false;
    *out = *in;
    out->Format = DXGI_FORMAT_R16G16_SNORM;
    ++g_promoted;
    logf("velocity_format: %ux%u R8G8_SNORM render target created as R16G16_SNORM", in->Width, in->Height);
    return true;
}

DXGI_FORMAT view_format(ID3D11Resource* res, DXGI_FORMAT in) {
    if (in != DXGI_FORMAT_R8G8_SNORM || !res)
        return in;
    D3D11_RESOURCE_DIMENSION dim;
    res->GetType(&dim);
    if (dim != D3D11_RESOURCE_DIMENSION_TEXTURE2D)
        return in;
    D3D11_TEXTURE2D_DESC d;
    static_cast<ID3D11Texture2D*>(res)->GetDesc(&d);
    return d.Format == DXGI_FORMAT_R16G16_SNORM ? DXGI_FORMAT_R16G16_SNORM : in;
}

}  // namespace kc::velocity_format
