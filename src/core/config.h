#pragma once
#include <windows.h>

#include <string>

namespace kc {

// kcd_clarity.ini next to the DLL. Missing keys get defaults; the file is written back on load
// so every setting is visible to the user.
struct Config {
    int mode = 1;              // [general] mode: 0 = engine TAA, 1 = DLSS
    UINT toggle_key = VK_F8;   // [ui] toggle_key, virtual-key code: settings menu
    UINT debug_key = VK_F3;    // [ui] debug_key: debug window
    bool show_on_start = false;
    bool show_fps = false;     // small FPS readout while the overlay is closed
    int dlss_preset = 0;       // [dlss] preset: NGX render preset, 0 = DLSS default, 10-13 = J-M
    bool d3d_debug = false;    // [debug] d3d_debug: D3D11 debug layer, messages to the mod log (slow)
};

Config& config();
void config_load(const std::wstring& path);
void config_save();

}  // namespace kc
