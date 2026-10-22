#pragma once
#include <cstdint>
#include <string>

namespace kc {

struct State {
    uintptr_t game_base = 0;       // WHGame.dll (engine + game code)
    uint32_t game_timestamp = 0;
    bool hooks_installed = false;
    std::wstring mod_dir;  // folder of our DLL (Bin\Win64), with trailing separator
    std::string wine_version;  // Wine/Proton version when running under it, empty on Windows
};

State& state();

}  // namespace kc
