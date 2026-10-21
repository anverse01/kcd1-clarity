# KCD Clarity

DLSS for Kingdom Come: Deliverance (the first game; Steam, DX11). Unofficial, not affiliated with Warhorse Studios or
NVIDIA.

**Work in progress.** The scene renders at a lower resolution and DLSS upscales it to native (or DLAA at 100%), replacing
the engine's TAA. Some passes still misbehave at reduced resolution; expect visual bugs.

## Requirements

- An NVIDIA RTX GPU.
- The Steam build of the game the symbols were taken from (`WHGame.dll` sha256 `cf9f6dc3…`). Engine functions are found
  by byte-pattern scan, so nearby builds may work, but other builds (GOG, older patches) are untested. The log and the
  F3 window say "UNKNOWN build" when the DLL doesn't match.

## Build

Windows, Visual Studio with the C++ x64 tools (CMake and Ninja come with it). Run `build.cmd`. CMake downloads Dear
ImGui, MinHook and the DLSS SDK files on the first build (see [THIRD_PARTY.md](THIRD_PARTY.md)).

From WSL, call `build.cmd` through a drive-letter path (e.g. `Z:\...`); a `\\wsl.localhost\` path doesn't link.

## Install

Copy `build\dinput8.dll` and `build\ngx\rel\nvngx_dlss.dll` into the game's `Bin\Win64`. To uninstall, delete both.

From WSL, `./deploy.sh` does this with the dev `nvngx_dlss.dll` (shows NVIDIA's DLSS debug overlay); `--remove` undoes
it. Set `KCD_BIN` if the game isn't in the default Steam location.

## Use

- **F8**: settings menu. **F3**: debug window.
- Settings: `Bin\Win64\kcd_clarity.ini`. Log: `Bin\Win64\kcd_clarity.log`.

## Development

Engine addresses and patterns come from a separate reverse-engineering workspace (`kre db export` writes
`src/generated/kcd_symbols.h`).

## License

MIT for the source in this repository; see [LICENSE](LICENSE). Third-party components keep their own licenses.
