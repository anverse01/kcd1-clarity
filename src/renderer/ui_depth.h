#pragma once
#include <cstdint>
#include <d3d11.h>
namespace kc::ui_depth {
void on_create_ps(const void *code, SIZE_T size, ID3D11PixelShader *shader);
void after_taa(UINT rw, UINT rh, UINT w, UINT h);
// Call under the hook's reentry guard. Only the identified native-resolution Scaleform pass qualifies.
void before_draw(ID3D11DeviceContext *ctx, ID3D11PixelShader *shader);
bool enabled();
void set_enabled(bool enabled);
uint64_t eligible_frames();
uint64_t corrected_frames();
} // namespace kc::ui_depth
