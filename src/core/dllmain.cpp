// Entry point. DllMain only sets up the dinput8.dll proxy and starts init_thread; everything else
// runs there, outside the loader lock.
//
// Why dinput8.dll: WHGame.dll imports DirectInput8Create, and the loader searches the application folder
// (Bin\Win64) before System32, so our copy there is loaded together with WHGame.dll.
//
// Timing: the game LoadLibrary's d3d11.dll itself when the renderer starts; DXGI hooks wait for that.
// WHGame.dll is mapped by then, so its symbols are resolved at the same point.
#include <windows.h>

#include <MinHook.h>

#include <string>

#include "core/config.h"
#include "core/crash.h"
#include "core/d3d_debug.h"
#include "core/log.h"
#include "core/proxy.h"
#include "core/state.h"
#include "game/symbols.h"
#include "hooks/d3d11_hooks.h"
#include "hooks/present.h"
#include "renderer/taa_dlss.h"

namespace kc {

static HMODULE g_self;
static State g_state;

State& state() { return g_state; }

static std::wstring self_dir() {
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(g_self, path, MAX_PATH);
    std::wstring s(path, n);
    return s.substr(0, s.find_last_of(L"\\/") + 1);
}

static bool is_game_process() {
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring s(path, n);
    std::wstring name = s.substr(s.find_last_of(L"\\/") + 1);
    return _wcsicmp(name.c_str(), L"KingdomCome.exe") == 0;
}

static DWORD WINAPI init_thread(LPVOID) {
    std::wstring dir = self_dir();
    g_state.mod_dir = dir;
    log_open(dir + L"kcd_clarity.log");
    logf("kcd-clarity %s, pid %lu", KC_VERSION, GetCurrentProcessId());
    config_load(dir + L"kcd_clarity.ini");
    taa_dlss::settings().mode = static_cast<taa_dlss::Mode>(config().mode);
    taa_dlss::settings().skip_ngx = config().skip_ngx;
    taa_dlss::settings().engine_after = config().engine_after;
    taa_dlss::settings().force_halton = config().force_halton;

    if (MH_Initialize() != MH_OK) {
        logf("MH_Initialize failed");
        return 0;
    }

    ULONGLONG start = GetTickCount64();
    while (!GetModuleHandleW(L"d3d11.dll"))
        Sleep(20);
    logf("d3d11.dll loaded after %llu ms", GetTickCount64() - start);

    sym::resolve_all();
    g_state.game_base = sym::status().module_base;
    g_state.game_timestamp = sym::status().module_timestamp;

    g_state.hooks_installed = install_present_hooks();
    d3d11::install();  // before the game creates its shaders
    d3d_debug::install();  // before the game creates its device
    // Installed last: the game may raise and handle its own exceptions during startup.
    install_crash_logger();
    logf("crash logger installed");
    return 0;
}

}  // namespace kc

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        kc::g_self = instance;
        DisableThreadLibraryCalls(instance);
        if (!kc::proxy_load())
            return FALSE;
        if (kc::is_game_process()) {
            HANDLE t = CreateThread(nullptr, 0, kc::init_thread, nullptr, 0, nullptr);
            if (t)
                CloseHandle(t);
        }
    }
    return TRUE;
}
