#pragma once
#include <d3d11.h>

#include <cstdint>

// Engine shader bug in the downscaled sub-rect path: GetLinearDepth(Texture2D, ScreenTC) (Common.cfi:262) loads at
// ScreenTC * PS_ScreenSize.xy, but ScreenTC is already scaled by HPosScale and PS_ScreenSize.xy is the scaled
// viewport, so the fog read depth at 0.667x the position (zoomed-in ghost, blocky edges). For those draws we bind a
// copy of the constant buffer with PS_ScreenSize.xy = full texture size. kcd-re db/notes/20-resolution.md.
namespace kc::screen_size_fix {

struct Shader {
    ID3D11PixelShader* shader = nullptr;  // identity only
    UINT cb_slot = ~0u;                   // constant buffer holding PS_ScreenSize
    UINT offset = 0;                      // byte offset of PS_ScreenSize in it
    UINT zmap_slot = ~0u;                 // zMap texture (its size is the full size)
};

// From CreatePixelShader: true (and *out filled) for a shader that needs the fix.
bool reflect(const void* code, SIZE_T size, Shader* out);

struct Status {
    bool enabled = true;
    uint32_t shaders_seen = 0;
    uint64_t draws_fixed = 0;
};
Status& status();

using DrawFn = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT);
// Render thread, from the Draw hook while such a shader is bound.
void draw(ID3D11DeviceContext* ctx, const Shader& s, UINT count, UINT start, DrawFn original);

}  // namespace kc::screen_size_fix
