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
    uint64_t ps_fixed = 0, vs_fixed = 0, shrunk = 0, non_screen = 0, bloom_fixed = 0;
};
Status& status();

using DrawFn = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT);
// Render thread, from the Draw hook, for a full-screen triangle whose UVs we scaled. The sub-rect invariant is "every
// screen buffer holds the current frame in its top-left (scale x scale) part":
//  - the bound VS scales the UVs itself (see above): Redraw with the engine's unscaled triangle;
//  - the viewport covers a whole screen-proportional target (SVO GI's 1/6-res passes, 426x240 on 426x240): Shrink
//    the viewport to the sub-rect. Keeping 1:1 UVs there preserved the layout, but the WPOS camera rays then spread
//    over the whole target while the depth covered the sub-rect: wrong world positions, GI "cloud" on near foliage;
//  - the viewport covers a whole non-screen target (2:1 sky textures, LUTs): Redraw unscaled;
//  - otherwise (the viewport already is the sub-rect): Draw as is.
enum class TriAction { Draw, Redraw, Shrink };
TriAction scaled_tri_action(ID3D11DeviceContext* ctx, const Shader* vs);
// Render thread, from the Draw hook while a fixed PS is bound (vs: the bound VS's entry, or nullptr).
void draw(ID3D11DeviceContext* ctx, const Shader& ps, const Shader* vs, UINT count, UINT start, DrawFn original);

// Bloom: the quarter-res scene (the luminance pass's source) holds the frame in its sub-rect, but the bloom blur
// passes run with g_VS_HPosScale.xy = 1 and the tonemap reads the bloom with unscaled UVs (o1 = v1), so it showed the
// sub-rect's bloom stretched 1/scale: a bright haze (the sunlit path's bloom) over the screen centre that shrank
// towards the top left with the scale. Fix: the first blur pass (a self-scaling VS reading that texture at t0) gets
// g_VS_HPosScale.xy = downscale, so it stretches the sub-rect over its whole target and the rest of the chain and the
// tonemap see a full-layout bloom. Returns false (nothing drawn) for any other draw.
bool draw_bloom_source(ID3D11DeviceContext* ctx, const Shader& vs, UINT count, UINT start, DrawFn original);

}  // namespace kc::screen_size_fix
