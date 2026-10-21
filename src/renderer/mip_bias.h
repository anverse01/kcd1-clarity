#pragma once
#include <d3d11.h>

// Texture LOD bias for the lower render resolution. With SetViewportDownscale the scene is rasterised at
// render size, so the hardware picks mips for that size and DLSS gets (and keeps) blurrier texture detail.
// Every sampler state does MipLODBias 0 (kcd_frame8574, kcd-re notes/20-resolution.md), so we swap the
// material samplers bound through PSSetSamplers for clones with bias log2(render/output) + user offset.
namespace kc::mip_bias {

// Render thread, every Present: bias to apply from now on (0 = pass the game's samplers through).
void set_bias(float bias);
float bias();

// PSSetSamplers hook: fills `out` with the samplers to bind instead of `in`. False = bind `in` unchanged.
bool remap(ID3D11DeviceContext* ctx, UINT count, ID3D11SamplerState* const* in, ID3D11SamplerState** out);

struct Status {
    unsigned clones = 0;        // biased samplers currently cached
    unsigned long long remapped = 0;  // sampler bindings replaced
};
const Status& status();

}  // namespace kc::mip_bias
