#pragma once
#include <d3d11.h>

#include <cstdint>

// Device/context-level hooks in d3d11.dll (shared by every device created with the same flags):
//   CreatePixelShader: recognizes the engine's TAA resolve (PostAA.cfx PostAA_PS, 2TX path) by its reflection.
//   PSSetShader:       tracks the bound pixel shader.
//   PSSetSamplers:     renderer/mip_bias swaps in LOD-biased samplers below render scale 1.
//   Draw:              when the TAA shader draws, renderer/taa_dlss may replace the draw.
//   CreateComputeShader / CSSetShader / Dispatch: renderer/screen_size_fix patches the tiled shading CS.
//   CreateTexture2D / CreateShaderResourceView / CreateRenderTargetView: renderer/velocity_format (16-bit motion
//                      vectors) and renderer/shadow_noise (updatable shadow rotation noise).
//   All non-indirect draws, Dispatch, OMSetRenderTargets[AndUnorderedAccessViews]: renderer/canary (edge clamp
//                      or debug fill outside the sub-rect).
namespace kc::d3d11 {

// What the TAA pixel shader expects where, read from its reflection when it is created.
struct TaaShader {
    ID3D11PixelShader* shader = nullptr;  // identity only
    UINT slot_velocity = ~0u;  // _tex3: object velocity (R8G8, encoded)
    UINT slot_colour = ~0u;    // _tex4: current frame (after SMAA)
    UINT slot_history = ~0u;   // _tex5
    UINT slot_depth = ~0u;     // PostAA_DepthTex: device depth
    UINT cb_slot = ~0u;        // constant buffer holding mReprojection
    UINT reproj_offset = 0;    // byte offset of mReprojection in it
    bool reproj_row_major = false;
};

// Init thread, once d3d11.dll is loaded and before the game creates its device.
bool install();

struct Status {
    bool installed = false;
    uint32_t taa_shaders_seen = 0;  // TAA pixel shaders created by the game
    uint64_t taa_draws = 0;         // TAA draws seen
    uint64_t taa_replaced = 0;      // ... of which renderer/taa_dlss replaced
};
const Status& status();
const TaaShader* last_taa_shader();

// While alive, draws and shader creation on this thread bypass the hooks (our own passes).
struct ScopedInside {
    ScopedInside();
    ~ScopedInside();
};

}  // namespace kc::d3d11
