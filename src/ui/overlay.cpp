// ImGui overlay drawn into the game's back buffer just before Present: the settings menu (F8) for players
// and the debug window (F3).
//
// Threads: only on_present (render thread) calls ImGui. wndproc just queues the messages ImGui
// needs; on_present replays them before NewFrame with no lock held. Calling ImGui from wndproc
// under a lock deadlocked: ImGui's SetCapture re-enters wndproc synchronously (WM_CAPTURECHANGED).
//
// Mouse in gameplay: the game reads the mouse through raw input and gets no legacy mouse messages then,
// so clicks and the wheel are also taken from WM_INPUT while no legacy mouse message arrives, and the
// cursor is freed by ui/input.cpp. If the OS cursor still does not move, the overlay cursor follows the
// raw mouse motion instead.
#include "ui/overlay.h"

#include <windows.h>

#include <d3d11.h>

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <algorithm>
#include <atomic>
#include <iterator>
#include <mutex>
#include <vector>

#include "core/config.h"
#include "core/log.h"
#include "core/state.h"
#include "game/renderer.h"
#include "game/symbols.h"
#include "hooks/d3d11_hooks.h"
#include "renderer/taa_dlss.h"
#include "upscaler/dlss.h"
#include "ui/input.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace kc::overlay {

// Guards g_rtv and ImGui rendering. Recursive: ResizeBuffers can re-enter on the render thread.
static std::recursive_mutex g_render_mutex;
static bool g_initialized;
static bool g_failed;
static IDXGISwapChain* g_swapchain;  // identity only, no reference held
static HWND g_window;
static WNDPROC g_game_wndproc;
static ID3D11Device* g_device;
static ID3D11DeviceContext* g_context;
static ID3D11RenderTargetView* g_rtv;
static std::atomic<bool> g_menu_visible;   // F8
static std::atomic<bool> g_debug_visible;  // F3
static bool g_menu_key_down, g_debug_key_down;

static uint64_t g_frames;
static LARGE_INTEGER g_qpc_freq, g_qpc_last;
static float g_frame_ms;

constexpr ULONGLONG kLegacyMouseQuietMs = 500;  // no legacy mouse message for this long: use raw input

struct QueuedMessage {
    UINT msg;
    WPARAM wparam;
    LPARAM lparam;
    // msg == WM_INPUT: the raw mouse report, read on the window thread (the handle dies with the message).
    USHORT raw_buttons;
    SHORT raw_wheel;
    LONG raw_dx, raw_dy;
};
static std::mutex g_queue_mutex;  // only ever held for a push or a swap, never around Win32 calls
static std::vector<QueuedMessage> g_queue;
static std::atomic<DWORD> g_wndproc_thread;
static std::atomic<ULONGLONG> g_last_legacy_mouse;

// Input diagnostics for the debug window.
static struct {
    uint32_t raw_mouse = 0;     // WM_INPUT mouse reports while open
    uint32_t legacy_mouse = 0;  // legacy mouse messages while open
    bool using_raw = false;     // clicks / wheel taken from raw input last frame
    bool virtual_cursor = false;
    float vx = 0, vy = 0;       // virtual cursor, client pixels
    POINT last_real{};
    int stuck_frames = 0;
    uint32_t os_cursor_fed = 0;  // frames the OS cursor position was fed to ImGui by us
} g_in;

static bool is_input_message(UINT msg) {
    return (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) || (msg >= WM_KEYFIRST && msg <= WM_KEYLAST) ||
           msg == WM_INPUT;
}

static bool is_legacy_mouse(UINT msg) { return msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST; }

// Messages ImGui's Win32 backend consumes (WM_INPUT is translated separately).
static bool imgui_wants(UINT msg) {
    return (is_input_message(msg) && msg != WM_INPUT) || msg == WM_MOUSELEAVE || msg == WM_SETFOCUS ||
           msg == WM_KILLFOCUS;
}

static bool any_visible() { return g_menu_visible || g_debug_visible; }

static void queue(const QueuedMessage& m) {
    std::lock_guard lock(g_queue_mutex);
    if (g_queue.size() < 1024)
        g_queue.push_back(m);
}

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (!g_wndproc_thread.exchange(GetCurrentThreadId()))
        logf("overlay: window messages on thread %lu", GetCurrentThreadId());
    if (any_visible()) {
        if (is_legacy_mouse(msg))
            g_last_legacy_mouse = GetTickCount64();
        if (imgui_wants(msg)) {
            queue({msg, wparam, lparam, 0, 0, 0, 0});
        } else if (msg == WM_INPUT) {
            RAWINPUT ri;
            UINT size = sizeof(ri);
            if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lparam), RID_INPUT, &ri, &size, sizeof(RAWINPUTHEADER)) !=
                    static_cast<UINT>(-1) &&
                ri.header.dwType == RIM_TYPEMOUSE) {
                const RAWMOUSE& m = ri.data.mouse;
                bool relative = (m.usFlags & MOUSE_MOVE_ABSOLUTE) == 0;
                queue({msg, 0, 0, m.usButtonFlags, static_cast<SHORT>(m.usButtonData), relative ? m.lLastX : 0,
                       relative ? m.lLastY : 0});
            }
        }
        // While the overlay is open the game gets no mouse/keyboard input.
        if (is_input_message(msg))
            return msg == WM_INPUT ? DefWindowProcW(hwnd, msg, wparam, lparam) : 0;
    }
    return CallWindowProcW(g_game_wndproc, hwnd, msg, wparam, lparam);
}

static void raw_buttons(ImGuiIO& io, USHORT flags, SHORT wheel) {
    static const struct { USHORT down, up; int button; } kButtons[] = {
        {RI_MOUSE_LEFT_BUTTON_DOWN, RI_MOUSE_LEFT_BUTTON_UP, ImGuiMouseButton_Left},
        {RI_MOUSE_RIGHT_BUTTON_DOWN, RI_MOUSE_RIGHT_BUTTON_UP, ImGuiMouseButton_Right},
        {RI_MOUSE_MIDDLE_BUTTON_DOWN, RI_MOUSE_MIDDLE_BUTTON_UP, ImGuiMouseButton_Middle}};
    for (const auto& b : kButtons) {
        if (flags & b.down)
            io.AddMouseButtonEvent(b.button, true);
        if (flags & b.up)
            io.AddMouseButtonEvent(b.button, false);
    }
    if (flags & RI_MOUSE_WHEEL)
        io.AddMouseWheelEvent(0.0f, float(wheel) / float(WHEEL_DELTA));
}

// Feeds queued window messages to ImGui on the render thread. No lock is held while ImGui runs, so
// anything it does that re-enters wndproc (SetCapture → WM_CAPTURECHANGED) only queues.
// Returns the summed raw mouse motion.
static POINT drain_messages() {
    std::vector<QueuedMessage> messages;
    {
        std::lock_guard lock(g_queue_mutex);
        messages.swap(g_queue);
    }
    ImGuiIO& io = ImGui::GetIO();
    g_in.using_raw = GetTickCount64() - g_last_legacy_mouse > kLegacyMouseQuietMs;
    POINT motion{};
    for (const QueuedMessage& m : messages) {
        if (m.msg != WM_INPUT) {
            if (is_legacy_mouse(m.msg))
                ++g_in.legacy_mouse;
            ImGui_ImplWin32_WndProcHandler(g_window, m.msg, m.wparam, m.lparam);
            continue;
        }
        ++g_in.raw_mouse;
        motion.x += m.raw_dx;
        motion.y += m.raw_dy;
        if (g_in.using_raw)
            raw_buttons(io, m.raw_buttons, m.raw_wheel);
    }
    return motion;
}

// After the Win32 backend's NewFrame: if the OS cursor stays put while the mouse moves (the game keeps
// it pinned some other way), drive the overlay cursor from the raw motion.
static void update_virtual_cursor(POINT motion) {
    POINT real{};
    if (!input::real_cursor_pos(&real) || !ScreenToClient(g_window, &real))
        return;
    bool moved = real.x != g_in.last_real.x || real.y != g_in.last_real.y;
    if (!g_in.virtual_cursor) {
        g_in.stuck_frames = (motion.x || motion.y) && !moved ? g_in.stuck_frames + 1 : 0;
        if (g_in.stuck_frames >= 3) {
            g_in.virtual_cursor = true;
            g_in.vx = float(real.x);
            g_in.vy = float(real.y);
            logf("overlay: OS cursor does not move with the mouse, using a virtual cursor");
        }
    }
    g_in.last_real = real;
    if (!g_in.virtual_cursor) {
        // The Win32 backend stops polling the cursor once it has seen a WM_MOUSEMOVE and then waits for
        // more; when the game switches to raw input only (gameplay, e.g. after a death) none come and the
        // overlay cursor froze while the OS cursor moved. Feed the real position ourselves.
        if (moved) {
            ImGui::GetIO().AddMousePosEvent(float(real.x), float(real.y));
            ++g_in.os_cursor_fed;
        }
        return;
    }
    RECT rc{};
    GetClientRect(g_window, &rc);
    g_in.vx = std::clamp(g_in.vx + float(motion.x), 0.0f, float(std::max<LONG>(rc.right - 1, 0)));
    g_in.vy = std::clamp(g_in.vy + float(motion.y), 0.0f, float(std::max<LONG>(rc.bottom - 1, 0)));
    ImGui::GetIO().AddMousePosEvent(g_in.vx, g_in.vy);
}

static UINT g_gpu_vendor;
static char g_gpu_name[128];

static void read_gpu(ID3D11Device* device) {
    IDXGIDevice* dxgi = nullptr;
    if (FAILED(device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgi))))
        return;
    IDXGIAdapter* adapter = nullptr;
    if (SUCCEEDED(dxgi->GetAdapter(&adapter))) {
        DXGI_ADAPTER_DESC d;
        if (SUCCEEDED(adapter->GetDesc(&d))) {
            g_gpu_vendor = d.VendorId;
            WideCharToMultiByte(CP_UTF8, 0, d.Description, -1, g_gpu_name, sizeof(g_gpu_name), nullptr, nullptr);
        }
        adapter->Release();
    }
    dxgi->Release();
    logf("overlay: GPU %s, vendor 0x%04x", g_gpu_name, g_gpu_vendor);
}

static bool init(IDXGISwapChain* swapchain) {
    if (FAILED(swapchain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&g_device)))) {
        logf("overlay: swapchain has no D3D11 device, overlay disabled");
        return false;
    }
    g_device->GetImmediateContext(&g_context);
    read_gpu(g_device);
    DXGI_SWAP_CHAIN_DESC desc;
    swapchain->GetDesc(&desc);
    g_window = desc.OutputWindow;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;  // no imgui.ini in the game folder; settings live in kcd_clarity.ini
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(g_window);
    ImGui_ImplDX11_Init(g_device, g_context);
    input::install();

    g_game_wndproc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(g_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&wndproc)));
    g_swapchain = swapchain;
    g_menu_visible = config().show_on_start;
    QueryPerformanceFrequency(&g_qpc_freq);
    QueryPerformanceCounter(&g_qpc_last);
    logf("overlay: initialized, window %p, back buffer %ux%u fmt %d, menu key 0x%x, debug key 0x%x", g_window,
         desc.BufferDesc.Width, desc.BufferDesc.Height, (int)desc.BufferDesc.Format, config().toggle_key,
         config().debug_key);
    return true;
}

static void poll_key(UINT key, bool* was_down, std::atomic<bool>* visible) {
    bool down = GetForegroundWindow() == g_window && (GetAsyncKeyState((int)key) & 0x8000);
    if (down && !*was_down)
        *visible = !*visible;
    *was_down = down;
}

static void update_timing() {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    float ms = float(now.QuadPart - g_qpc_last.QuadPart) * 1000.0f / float(g_qpc_freq.QuadPart);
    g_qpc_last = now;
    g_frame_ms = g_frame_ms == 0.0f ? ms : g_frame_ms * 0.95f + ms * 0.05f;
    ++g_frames;
}

static void draw_fps_corner() {
    ImGui::SetNextWindowPos(ImVec2(8, 8));
    ImGui::SetNextWindowBgAlpha(0.4f);
    ImGui::Begin("##fps", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    ImGui::Text("%.1f fps  %.2f ms", g_frame_ms > 0 ? 1000.0f / g_frame_ms : 0.0f, g_frame_ms);
    ImGui::End();
}

static const ImVec4 kGood(0.4f, 1, 0.4f, 1), kWarn(1, 0.8f, 0.3f, 1), kBad(1, 0.4f, 0.3f, 1);

static void draw_menu_window() {
    ImGui::SetNextWindowPos(ImVec2(40, 40), ImGuiCond_FirstUseEver);
    ImGui::Begin("KCD Clarity", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
    const sym::Status& ss = sym::status();
    if (ss.missing)
        ImGui::TextColored(kBad, "%d engine symbols missing: inactive (see kcd_clarity.log)", ss.missing);
    else
        ImGui::TextColored(kGood, "Engine symbols resolved (%d)", ss.resolved);
    ImGui::TextDisabled("Early development build: no upscaling yet. F3 opens the probe/debug window.");
    ImGui::End();
}

static void draw_dlss_section() {
    taa_dlss::Settings& s = taa_dlss::settings();
    const taa_dlss::Status& st = taa_dlss::status();
    const dlss::Status& d = dlss::status();
    const d3d11::Status& h = d3d11::status();
    const game::Snapshot& snap = game::snapshot();

    int mode = static_cast<int>(s.mode);
    bool changed = ImGui::RadioButton("Engine TAA", &mode, 0);
    ImGui::SameLine();
    changed |= ImGui::RadioButton("DLAA", &mode, 1);
    if (changed) {
        s.mode = static_cast<taa_dlss::Mode>(mode);
        config().mode = mode;
        config_save();
        taa_dlss::request_reset();
    }
    if (snap.valid && snap.aa_mode_active != 3)
        ImGui::TextColored(kWarn, "Anti-aliasing is not SMAA 2TX (r_AntialiasingMode 3): the TAA pass does not run");

    ImGui::Text("D3D11 hooks %s; TAA shaders seen %u; TAA draws %llu, replaced %llu", h.installed ? "ok" : "MISSING",
                h.taa_shaders_seen, (unsigned long long)h.taa_draws, (unsigned long long)h.taa_replaced);
    if (const d3d11::TaaShader* t = d3d11::last_taa_shader())
        ImGui::TextDisabled("TAA PS: colour t%u depth t%u velocity t%u, mReprojection cb%u+%u %s", t->slot_colour,
                            t->slot_depth, t->slot_velocity, t->cb_slot, t->reproj_offset,
                            t->reproj_row_major ? "row-major" : "column-major");
    ImGui::Text("NGX: %s, feature %s (0x%08x), last eval 0x%08x, %llu evaluations",
                !d.init_tried ? "not initialised" : d.available ? "DLSS available" : "DLSS NOT available",
                d.feature_created ? "created" : "none", d.create_result, d.last_eval_result,
                (unsigned long long)d.evaluations);
    if (st.last_ok)
        ImGui::TextColored(kGood, "DLSS %ux%u -> %ux%u, jitter (%+.3f, %+.3f) px", st.render_width, st.render_height,
                           st.target_width, st.target_height, st.jitter_x, st.jitter_y);
    else if (*st.last_error)
        ImGui::TextColored(kBad, "Fallback: %s", st.last_error);

    ImGui::SeparatorText("Conventions (to verify in game)");
    bool reset = false;
    reset |= ImGui::Checkbox("Flip jitter X", &s.jitter_flip_x);
    ImGui::SameLine();
    reset |= ImGui::Checkbox("Flip jitter Y", &s.jitter_flip_y);
    reset |= ImGui::Checkbox("Flip MV X", &s.mv_flip_x);
    ImGui::SameLine();
    reset |= ImGui::Checkbox("Flip MV Y", &s.mv_flip_y);
    ImGui::SetNextItemWidth(200);
    reset |= ImGui::Combo("Depth", &s.depth_mode, "auto (r_ReverseDepth)\0normal\0inverted\0");
    ImGui::SameLine();
    ImGui::TextDisabled("r_ReverseDepth = %d", st.reverse_depth);
    reset |= ImGui::Checkbox("MVs include jitter (MVJittered)", &dlss::settings().mv_jittered);
    reset |= ImGui::Checkbox("Force Halton jitter (pattern 8)", &s.force_halton);
    if (ImGui::Button("Reset DLSS history"))
        reset = true;
    if (reset)
        taa_dlss::request_reset();
    ImGui::SetNextItemWidth(160);
    if (ImGui::SliderInt("DLSS preset (0 = default, 10-13 = J-M)", &config().dlss_preset, 0, 13)) {
        config_save();
        dlss::request_recreate();
    }
}

static void draw_symbols_section() {
    const sym::Status& ss = sym::status();
    ImGui::Text("WHGame.dll 0x%llx, timestamp 0x%08x: %s", (unsigned long long)ss.module_base, ss.module_timestamp,
                ss.known_build ? "known build" : "UNKNOWN build");
    ImGui::Text("%d resolved (%d pattern, %d rva), %d missing", ss.resolved, ss.by_pattern, ss.by_rva, ss.missing);
    if (ImGui::TreeNode("All symbols")) {
        for (size_t i = 0; i < std::size(kcd::sym::kSymbols); ++i) {
            uintptr_t a = sym::addr(static_cast<sym::Id>(i));
            ImGui::Text("%-44s %s", kcd::sym::kSymbols[i].name,
                        a ? "" : "MISSING");
            if (a) {
                ImGui::SameLine();
                ImGui::TextDisabled("+0x%llx", (unsigned long long)(a - ss.module_base));
            }
        }
        ImGui::TreePop();
    }
}

// Experiments from kcd-re: dynamic resolution via SetViewportDownscale, TAA jitter readback.
static void draw_probe_section() {
    if (!game::ready()) {
        ImGui::TextColored(kBad, "gRenDev not resolved");
        return;
    }
    const game::Snapshot& s = game::snapshot();
    game::Probe& p = game::probe();
    ImGui::Text("Render size %dx%d, viewport %dx%d", s.render_width, s.render_height, s.viewport_width,
                s.viewport_height);
    ImGui::Text("Downscale req (%.3f, %.3f)  cur (%.3f, %.3f)  prev (%.3f, %.3f)", s.downscale_req.x,
                s.downscale_req.y, s.downscale_cur.x, s.downscale_cur.y, s.downscale_prev.x, s.downscale_prev.y);
    ImGui::Text("Viewport scale at Present (%.3f, %.3f)", s.viewport_scale.x, s.viewport_scale.y);
    ImGui::Checkbox("Force SetViewportDownscale", &p.force_downscale);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160);
    ImGui::SliderFloat("##scale", &p.downscale, 0.33f, 1.0f, "%.3f");
    ImGui::Separator();
    ImGui::Text("r_AntialiasingMode %d (active %d), TAA pattern %d, frame index %u", s.aa_mode, s.aa_mode_active,
                s.taa_pattern, s.frame_index);
    ImGui::Text("Jitter clip (%+.6f, %+.6f)  ~px (%+.3f, %+.3f)", s.jitter_clip.x, s.jitter_clip.y, s.jitter_px.x,
                s.jitter_px.y);
    static const char* const kPatterns[] = {"leave", "0 none", "1 2x", "2 3x", "3 4x", "4 8x", "5 sparse 8x8",
                                            "6 random", "7 Halton 8x", "8 Halton 1024"};
    int sel = p.force_pattern + 1;
    ImGui::SetNextItemWidth(200);
    if (ImGui::Combo("Force TAA pattern", &sel, kPatterns, IM_ARRAYSIZE(kPatterns)))
        p.force_pattern = sel - 1;
    int n = 0;
    const game::Vec2* h = game::jitter_history(&n);
    if (n && ImGui::TreeNode("Jitter history (px, oldest first)")) {
        for (int i = 0; i < n; ++i)
            ImGui::Text("%2d  %+.4f %+.4f", i, h[i].x, h[i].y);
        ImGui::TreePop();
    }
}

static void draw_input_section() {
    const input::Status& is = input::status();
    ImGui::Text("Cursor hooks: %s, captured %d", is.hooked ? "ok" : "NOT INSTALLED", is.captured);
    ImGui::Text("Game while open: SetCursorPos %u, ClipCursor %u, GetCursorPos %u", is.game_set_cursor,
                is.game_clip_cursor, is.game_get_cursor);
    ImGui::Text("Mouse reports: raw %u, legacy %u; clicks from %s", g_in.raw_mouse, g_in.legacy_mouse,
                g_in.using_raw ? "raw input" : "window messages");
    ImGui::Text("Cursor: %s, OS position fed %u frames", g_in.virtual_cursor ? "virtual (OS cursor pinned)" : "OS cursor",
                g_in.os_cursor_fed);
}

static void draw_debug_window(IDXGISwapChain* swapchain) {
    ImGui::SetNextWindowPos(ImVec2(480, 40), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(620, 0), ImGuiCond_FirstUseEver);
    ImGui::Begin("KCD Clarity debug " KC_VERSION);

    const State& s = state();
    if (ImGui::CollapsingHeader("Status", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("WHGame.dll 0x%llx, PE timestamp 0x%08x", (unsigned long long)s.game_base, s.game_timestamp);
        ImGui::Text("GPU %s (vendor 0x%04x)", g_gpu_name, g_gpu_vendor);
        DXGI_SWAP_CHAIN_DESC desc;
        if (SUCCEEDED(swapchain->GetDesc(&desc)))
            ImGui::Text("Back buffer %ux%u, format %d, %u buffers", desc.BufferDesc.Width, desc.BufferDesc.Height,
                        (int)desc.BufferDesc.Format, desc.BufferCount);
        ImGui::Text("Frame %llu, %.1f fps (%.2f ms)", (unsigned long long)g_frames,
                    g_frame_ms > 0 ? 1000.0f / g_frame_ms : 0.0f, g_frame_ms);
        ImGui::TextDisabled("D3D11 debug layer: [debug] d3d_debug=1 in kcd_clarity.ini (restart)");
    }
    if (ImGui::CollapsingHeader("DLSS", ImGuiTreeNodeFlags_DefaultOpen))
        draw_dlss_section();
    if (ImGui::CollapsingHeader("Engine symbols"))
        draw_symbols_section();
    if (ImGui::CollapsingHeader("Renderer probe"))
        draw_probe_section();
    if (ImGui::CollapsingHeader("Overlay input"))
        draw_input_section();
    ImGui::End();
}

// ---------------------------------------------------------------------------------------------------------

void on_present(IDXGISwapChain* swapchain) {
    if (g_failed)
        return;
    if (!g_initialized) {
        g_initialized = init(swapchain);
        g_failed = !g_initialized;
        if (g_failed)
            return;
    }
    if (swapchain != g_swapchain) {
        DXGI_SWAP_CHAIN_DESC desc;
        if (FAILED(swapchain->GetDesc(&desc)) || desc.OutputWindow != g_window)
            return;  // some other swapchain (not the game window)
        logf("overlay: game window has a new swapchain %p", swapchain);
        on_resize_begin();
        g_swapchain = swapchain;
    }

    update_timing();
    poll_key(config().toggle_key, &g_menu_key_down, &g_menu_visible);
    poll_key(config().debug_key, &g_debug_key_down, &g_debug_visible);
    bool menu = g_menu_visible, debug = g_debug_visible, visible = menu || debug;
    if (visible && !input::status().captured) {  // just opened
        g_in.virtual_cursor = false;
        g_in.stuck_frames = 0;
        g_in.raw_mouse = g_in.legacy_mouse = g_in.os_cursor_fed = 0;
        input::real_cursor_pos(&g_in.last_real);
        ScreenToClient(g_window, &g_in.last_real);
    }
    if (!visible && input::status().captured)  // just closed
        logf("overlay: input while open: mouse reports raw %u, legacy %u; cursor %s, OS position fed %u frames",
             g_in.raw_mouse, g_in.legacy_mouse, g_in.virtual_cursor ? "virtual" : "OS", g_in.os_cursor_fed);
    input::set_captured(visible);
    if (!visible && !config().show_fps)
        return;

    POINT motion = drain_messages();
    std::lock_guard lock(g_render_mutex);
    if (!g_rtv) {
        ID3D11Texture2D* back_buffer = nullptr;
        if (FAILED(swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back_buffer))))
            return;
        HRESULT hr = g_device->CreateRenderTargetView(back_buffer, nullptr, &g_rtv);
        back_buffer->Release();
        if (FAILED(hr)) {
            logf("overlay: CreateRenderTargetView failed 0x%08lx", hr);
            return;
        }
    }

    ImGui::GetIO().MouseDrawCursor = visible;
    ImGui_ImplDX11_NewFrame();
    {
        input::OwnCalls own;  // the backend's cursor calls see the real cursor
        ImGui_ImplWin32_NewFrame();
    }
    if (visible)
        update_virtual_cursor(motion);
    ImGui::NewFrame();
    if (menu)
        draw_menu_window();
    if (debug)
        draw_debug_window(swapchain);
    if (!visible)
        draw_fps_corner();
    ImGui::Render();

    // The DX11 backend restores pipeline state itself, except the render targets we bind.
    ID3D11RenderTargetView* saved_rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
    ID3D11DepthStencilView* saved_dsv = nullptr;
    g_context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, saved_rtvs, &saved_dsv);
    g_context->OMSetRenderTargets(1, &g_rtv, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    g_context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, saved_rtvs, saved_dsv);
    for (auto* rtv : saved_rtvs)
        if (rtv)
            rtv->Release();
    if (saved_dsv)
        saved_dsv->Release();
}

void on_resize_begin() {
    std::lock_guard lock(g_render_mutex);
    if (g_rtv) {
        g_rtv->Release();
        g_rtv = nullptr;
    }
}

}  // namespace kc::overlay
