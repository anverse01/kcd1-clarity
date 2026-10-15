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

struct Shader {
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
    uint64_t ps_fixed = 0, vs_fixed = 0;
};
Status& status();

using DrawFn = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT);
// Render thread, from the Draw hook. Returns true if the draw must be skipped (self-scaling VS on a scaled triangle,
// which is then redrawn unscaled).
bool skip_for_vs(const Shader* vs);
// Render thread, from the Draw hook while a fog PS is bound.
void draw(ID3D11DeviceContext* ctx, const Shader& ps, UINT count, UINT start, DrawFn original);

}  // namespace kc::screen_size_fix
