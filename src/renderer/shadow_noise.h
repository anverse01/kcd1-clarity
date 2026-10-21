#pragma once
#include <d3d11.h>
#include <dxgi.h>

// Temporal shadow filter noise. The sun shadow PCF (ShadowMaskGen PS, kcd-re notes/20-resolution.md) rotates its taps
// by a 64x64 R16G16_SNORM noise texture (sRotSampler) addressed in shadow space, so the pattern is fixed per world
// point. Undersampled at the render resolution it flickers when the camera moves, and DLSS cannot average a pattern
// that does not change. Fix: while DLSS runs, every frame rewrite the texture (all mips) with its vectors rotated by
// a further golden angle, so the kernel rotation varies over time and DLSS integrates it into a smooth penumbra.
// The texture is created IMMUTABLE by the engine; it is created DEFAULT instead so it can be updated.
namespace kc::shadow_noise {

// CreateTexture2D hook, before creation: true if *out (the desc to create with) differs from *in.
bool adjust_texture(const D3D11_TEXTURE2D_DESC* in, const D3D11_SUBRESOURCE_DATA* init, D3D11_TEXTURE2D_DESC* out);
// After a successful creation with an adjusted desc.
void on_created(ID3D11Texture2D* tex);

// Render thread, every Present: whether to animate this frame (false restores the engine's data once).
void set_active(bool on);
// Noise frequency divisor (1 = engine). Below ~half resolution the point-sampled, shadow-texel-anchored noise has
// more texels than render pixels: it aliases and crawls as the camera turns (shimmering penumbrae under DLSS).
void set_coarseness(int k);
void on_present(IDXGISwapChain* swapchain);

bool found();

}  // namespace kc::shadow_noise
