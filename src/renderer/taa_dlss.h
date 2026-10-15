#pragma once
#include <d3d11.h>

#include <cstdint>

#include "hooks/d3d11_hooks.h"

// Replaces the engine's TAA resolve draw (PostAA_PS, SMAA 2TX) with DLSS (DLAA at native resolution for now).
//
// At the TAA draw the engine has bound everything DLSS needs (kcd-re db/notes/10-taa.md, 30-frame.md):
//   _tex4 current frame (after SMAA), PostAA_DepthTex device depth, _tex3 object velocity,
//   mReprojection (current uv+depth -> previous clip) in the PS constant buffer, RTV = TAA output.
// We build full-screen motion vectors exactly like PostAA_PS does (camera reprojection where no object velocity),
// run DLSS, and write the result into the bound RTV with the same sRGB encoding the TAA shader uses.
namespace kc::taa_dlss {

enum class Mode { Engine = 0, Dlss = 1 };

struct Settings {
    Mode mode = Mode::Engine;
    bool force_halton = true;   // r_AntialiasingTAAPattern 8 (Halton 2,3 over 1024 frames) while DLSS is on
    bool jitter_flip_x = true;  // confirmed in game 2026-10-15: DLSS jitter = (-x, +y) of gRenDev+0xF20
    bool jitter_flip_y = false;
    bool mv_flip_x = false;
    bool mv_flip_y = false;
    int depth_mode = 0;         // 0 = from r_ReverseDepth, 1 = normal, 2 = inverted
    int debug_view = 0;         // 0 = DLSS (NaN magenta), 1 = colour input, 2 = motion vectors, 3 = depth
    // Bisection switches for the black-screen bug:
    bool skip_ngx = false;      // do everything except NGX evaluate (write back the colour input)
    bool engine_after = false;  // run our whole path, then let the engine's TAA draw run anyway (overwrites ours)
    bool state_swap = false;    // isolate with SwapDeviceContextState instead of a manual backup (breaks lighting)
};

struct Status {
    bool shaders_ready = false;
    bool resources_ready = false;
    bool last_ok = false;
    const char* last_error = "";
    UINT render_width = 0, render_height = 0, target_width = 0, target_height = 0;
    float jitter_x = 0, jitter_y = 0;  // pixels, as passed to DLSS
    int reverse_depth = -1;            // CV_r_ReverseDepth
    uint64_t frames = 0;
};

Settings& settings();
const Status& status();
void request_reset();

// Render thread, from the Draw hook while the TAA shader is bound. Returns true if the draw was replaced.
bool on_taa_draw(ID3D11DeviceContext* ctx, const d3d11::TaaShader& taa);
// Render thread, every Present: keeps the engine on the jitter pattern DLSS needs.
void on_present();

}  // namespace kc::taa_dlss
