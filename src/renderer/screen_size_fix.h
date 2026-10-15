#pragma once
#include <d3d11.h>

#include <cstdint>

// Constant-buffer fixes for draws in the downscaled sub-rect path (kcd-re db/notes/20-resolution.md). For an
// affected draw we bind a GPU copy of the shader's constant buffer with one float2 replaced, then put the engine's
// buffer back.
//
// Fog (pixel shader): GetLinearDepth(Texture2D, ScreenTC) (Common.cfi:262) loads at ScreenTC * PS_ScreenSize.xy,
// but ScreenTC is already scaled by HPosScale and PS_ScreenSize.xy is the scaled viewport: depth read at 0.667x the
// position (zoomed-in ghost, blocky edges). Fix: PS_ScreenSize.xy = zMap size.
//
// Self-scaling vertex shaders (multiply the input UV by g_VS_HPosScale, e.g. the tonemap VS: o3 = v1 * HPosScale
// for the scene, o1 = v1 for vignetting) already handle the sub-rect with the engine's unscaled triangle. With the
// triangle hook's scaled UVs the tonemap read the top-left 2/3 of the sub-rect (1.5x zoom). Fix: the Draw hook skips
// such a draw and the triangle is redrawn unscaled (game::redo_fullscreen_tri_unscaled).
namespace kc::screen_size_fix {

// Auto exposure (HDRSampleLumInitial): runs with the per-pass scale reset to 1 (full 64x64 viewport) but reads the
// quarter-res scene, whose current frame is only in the sub-rect; the rest is stale, so the measured luminance (and
// the exposure) was wrong: the image got darker. Fix: its VS's g_VS_HPosScale.xy = the frame's downscale factor.
struct Shader {
    enum Kind { FogPS, LumInitialPS, SelfScalingVS } kind = FogPS;
    void* shader = nullptr;  // ID3D11PixelShader* / ID3D11VertexShader*, identity only
    UINT cb_slot = ~0u;      // constant buffer holding the value to replace
    UINT offset = 0;         // byte offset of it
    UINT zmap_slot = ~0u;    // pixel shaders: zMap texture (its size is the full size)
};

// From CreatePixelShader / CreateVertexShader: true (and *out filled) for a shader that needs a fix.
bool reflect_ps(const void* code, SIZE_T size, Shader* out);
bool reflect_vs(const void* code, SIZE_T size, Shader* out);

struct Status {
    bool enabled = true;
    uint32_t ps_seen = 0, vs_seen = 0;
    uint64_t ps_fixed = 0, vs_fixed = 0, full_viewport_fixed = 0;
};
Status& status();

using DrawFn = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT);
// Render thread, from the Draw hook. Returns true if a scaled full-screen triangle must be redrawn with the engine's
// unscaled UVs instead (the draw is skipped):
//  - the bound VS scales the UVs itself (see above), or
//  - the viewport covers the whole render target. The sub-rect invariant is "every buffer holds the current frame in
//    its top-left (scale x scale) part"; a pass drawing its whole target (SVO GI's 1/6-res passes, 426x240 viewport on
//    426x240 targets) must map UVs 1:1 to keep that layout, or the next pass reads it at the wrong scale (broken GI,
//    darker image below 100%).
bool skip_scaled_tri(ID3D11DeviceContext* ctx, const Shader* vs);
// Render thread, from the Draw hook while a fixed PS is bound (vs: the bound VS's entry, or nullptr).
void draw(ID3D11DeviceContext* ctx, const Shader& ps, const Shader* vs, UINT count, UINT start, DrawFn original);

}  // namespace kc::screen_size_fix
