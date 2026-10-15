#include "game/renderer.h"

#include <cstring>

#include "core/log.h"
#include "game/symbols.h"

namespace kc::game {

// CD3D9Renderer field offsets (WHGame 1.9.x).
constexpr uintptr_t kJitterClip = 0xF20;
constexpr uintptr_t kViewportScale = 0x6764;
constexpr uintptr_t kRenderWidth = 0xEA90;
constexpr uintptr_t kRenderHeight = 0xEA94;
constexpr uintptr_t kDownscaleReq = 0xEF2C;
constexpr uintptr_t kDownscaleCur = 0xEF34;
constexpr uintptr_t kDownscalePrev = 0xEF3C;
constexpr uintptr_t kViewportWidth = 0xEF4C;
constexpr uintptr_t kViewportHeight = 0xEF50;

// IRenderer::SetViewportDownscale(float, float) returns Vec2 through a hidden pointer (CryEngine's Vec2 is not POD).
using SetViewportDownscaleFn = Vec2*(__fastcall*)(void* self, Vec2* ret, float x, float y);

static Probe g_probe;
static Snapshot g_snap;
static Vec2 g_history[kHistory];
static int g_history_count, g_history_next;
static uint32_t g_last_frame_index = ~0u;

Probe& probe() { return g_probe; }
const Snapshot& snapshot() { return g_snap; }

const Vec2* jitter_history(int* count) {
    static Vec2 ordered[kHistory];
    for (int i = 0; i < g_history_count; ++i)
        ordered[i] = g_history[(g_history_next - g_history_count + i + kHistory) % kHistory];
    *count = g_history_count;
    return ordered;
}

template <typename T>
static T field(uintptr_t base, uintptr_t offset) {
    T v;
    memcpy(&v, reinterpret_cast<const void*>(base + offset), sizeof(T));
    return v;
}

bool ready() { return sym::addr(sym::Id::gRenDev) != 0; }

void on_present() {
    uintptr_t r = sym::addr(sym::Id::gRenDev);
    if (!r)
        return;

    // The request is sticky in the engine, so turning the probe off has to ask for (1, 1) once.
    static bool was_forcing;
    if (g_probe.force_downscale || was_forcing) {
        if (auto fn = reinterpret_cast<SetViewportDownscaleFn>(sym::addr(sym::Id::CD3D9Renderer__SetViewportDownscale))) {
            float f = g_probe.force_downscale ? g_probe.downscale : 1.0f;
            Vec2 got;
            fn(reinterpret_cast<void*>(r), &got, f, f);
        }
        was_forcing = g_probe.force_downscale;
    }
    if (g_probe.force_pattern >= 0)
        if (int* p = sym::ptr<int>(sym::Id::CV_r_AntialiasingTAAPattern))
            *p = g_probe.force_pattern;

    Snapshot s;
    s.valid = true;
    s.render_width = field<int>(r, kRenderWidth);
    s.render_height = field<int>(r, kRenderHeight);
    s.viewport_width = field<int>(r, kViewportWidth);
    s.viewport_height = field<int>(r, kViewportHeight);
    s.downscale_req = field<Vec2>(r, kDownscaleReq);
    s.downscale_cur = field<Vec2>(r, kDownscaleCur);
    s.downscale_prev = field<Vec2>(r, kDownscalePrev);
    s.viewport_scale = field<Vec2>(r, kViewportScale);
    s.jitter_clip = field<Vec2>(r, kJitterClip);
    // Inverse of RT_RenderScene: clip = 2 * px / width / scale.
    s.jitter_px.x = s.jitter_clip.x * s.render_width * s.downscale_cur.x * 0.5f;
    s.jitter_px.y = s.jitter_clip.y * s.render_height * s.downscale_cur.y * 0.5f;
    if (int* p = sym::ptr<int>(sym::Id::CV_r_AntialiasingMode))
        s.aa_mode = *p;
    if (int* p = sym::ptr<int>(sym::Id::g_AntialiasingModeActive))
        s.aa_mode_active = *p;
    if (int* p = sym::ptr<int>(sym::Id::CV_r_AntialiasingTAAPattern))
        s.taa_pattern = *p;
    if (uint32_t* p = sym::ptr<uint32_t>(sym::Id::g_TAAFrameIndex))
        s.frame_index = *p;
    g_snap = s;

    if (s.frame_index != g_last_frame_index) {
        g_last_frame_index = s.frame_index;
        g_history[g_history_next] = s.jitter_px;
        g_history_next = (g_history_next + 1) % kHistory;
        if (g_history_count < kHistory)
            ++g_history_count;
    }
}

}  // namespace kc::game
