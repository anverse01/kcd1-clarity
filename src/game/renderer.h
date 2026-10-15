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
    bool fix_fullscreen_uv = true; // DrawFullScreenTri hook: scale UVs by the downscale factor
};

// Hooks into the engine (after sym::resolve_all). DrawFullScreenTri: when the scene renders into a downscaled
// sub-rect, the full-screen triangle's UVs must cover only that sub-rect (kcd-re notes/20-resolution.md).
void install_hooks();
uint64_t fullscreen_uv_fixes();  // draws whose UVs were scaled, for the debug window

bool ready();  // symbols needed for the snapshot are resolved
// Asks the engine to render the scene into a (scale x scale) sub-rect from the next frame on (sticky until
// changed). The F3 probe's force, when on, wins. Render thread.
void set_downscale(float scale);
// Downscale factor applied to the pass being rendered right now (+0x6764), (1, 1) outside the scene.
Vec2 current_viewport_scale();
// Downscale factor the scene of this frame renders with (+0xEF34), (1, 1) when not downscaled.
Vec2 current_downscale();
// Native render size (+0xEA90 / +0xEA94, unscaled).
Vec2 native_size();
// True while DrawFullScreenTri[WPOS] is drawing a triangle whose UVs we scaled (render thread).
bool inside_scaled_fullscreen_tri();
// From the Draw hook, inside such a triangle: the bound vertex shader scales the UVs itself (HPosScale), so the
// draw was skipped; the triangle is drawn again with the engine's own (unscaled) UVs.
void redo_fullscreen_tri_unscaled();
Probe& probe();
// Render thread, every Present: applies the probe and takes a snapshot.
void on_present();
const Snapshot& snapshot();
// Last kHistory jitter values in pixels (oldest first), to see the sequence and its period.
constexpr int kHistory = 32;
const Vec2* jitter_history(int* count);

}  // namespace kc::game
