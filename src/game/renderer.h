#pragma once
#include <cstdint>

// Read access to the engine's CD3D9Renderer (static instance gRenDev in WHGame.dll) and the first runtime
// experiments. Field offsets come from kcd-re (db/notes/20-resolution.md, 40-jitter.md).
namespace kc::game {

struct Vec2 {
    float x, y;
};

struct Snapshot {
    bool valid = false;
    int render_width = 0, render_height = 0;  // +0xEA90 / +0xEA94
    int viewport_width = 0, viewport_height = 0;  // +0xEF4C / +0xEF50, scaled by the current downscale factor
    Vec2 downscale_req{}, downscale_cur{}, downscale_prev{};  // +0xEF2C / +0xEF34 / +0xEF3C
    Vec2 viewport_scale{};    // +0x6764, factor applied to the pass that just ran
    Vec2 jitter_clip{};       // +0xF20, added to P[2][0] / P[2][1] in RT_SetCameraInfo
    Vec2 jitter_px{};         // jitter_clip converted back to render pixels
    int aa_mode = -1;         // CV_r_AntialiasingMode
    int aa_mode_active = -1;  // g_AntialiasingModeActive
    int taa_pattern = -1;     // CV_r_AntialiasingTAAPattern
    uint32_t frame_index = 0; // g_TAAFrameIndex
};

// Runtime experiments, driven from the debug window.
struct Probe {
    bool force_downscale = false;  // call SetViewportDownscale(scale, scale) every frame
    float downscale = 0.667f;
    int force_pattern = -1;        // -1 = leave CV_r_AntialiasingTAAPattern alone
};

bool ready();  // symbols needed for the snapshot are resolved
Probe& probe();
// Render thread, every Present: applies the probe and takes a snapshot.
void on_present();
const Snapshot& snapshot();
// Last kHistory jitter values in pixels (oldest first), to see the sequence and its period.
constexpr int kHistory = 32;
const Vec2* jitter_history(int* count);

}  // namespace kc::game
