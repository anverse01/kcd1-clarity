#pragma once
#include <d3d11.h>

// Object motion vectors at 16 bits. The engine's velocity target (the G-buffer's 4th target, sampled by the TAA as
// _tex3 and by our DLSS prepare pass) is R8G8_SNORM written with DecodeMotionVector's UNORM-style encoding
// (e = 0.5 + 0.5 * sign(v) * sqrt|v|), so only 7 bits are used: around 2 px/frame of motion one step is ~1.9 px,
// for a running animal ~9 px (kcd-re notes/10-taa.md). DLSS trusts the vectors, so animals and people vibrated
// while the camera turned slowly (pixelated below scale 1). Fix: the target is created as R16G16_SNORM, and the
// views the engine creates for it with an explicit R8G8_SNORM format get R16G16_SNORM. Nothing copies it.
namespace kc::velocity_format {

// Read once at startup ([dlss] velocity16); takes effect for targets created afterwards.
void set_enabled(bool on);

// CreateTexture2D hook: true if *desc was changed (create with it instead).
bool adjust_texture(const D3D11_TEXTURE2D_DESC* in, D3D11_TEXTURE2D_DESC* out);
// CreateShaderResourceView / CreateRenderTargetView hooks: the view format to use for `res` (in = the engine's).
DXGI_FORMAT view_format(ID3D11Resource* res, DXGI_FORMAT in);

unsigned promoted();  // targets created at 16 bits, for the debug window

}  // namespace kc::velocity_format
