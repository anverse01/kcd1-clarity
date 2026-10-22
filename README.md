<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="docs/assets/wordmark.png">
    <img src="docs/assets/wordmark-dark.png" alt="KCD1 Clarity" width="520">
  </picture>
</p>

# KCD1 Clarity

NVIDIA DLSS for **Kingdom Come: Deliverance** (the first game). It replaces the game's built-in anti-aliasing with
DLSS. That gives a cleaner, more stable image, and with upscaling, more FPS. The game renders at a lower resolution
and DLSS brings it back up to your screen's.

Unofficial mod, not affiliated with Warhorse Studios or NVIDIA. Uses NVIDIA DLSS.

> **Early version.** DLAA (100%) is the most reliable mode. Below 100% some effects can still look wrong. Please
> [report](#reporting-bugs) anything you see.

**Website:** [eonflux7.github.io/kcd1-clarity](https://eonflux7.github.io/kcd1-clarity/) ·
[Config guide & cvar database](https://eonflux7.github.io/kcd1-clarity/cvars/)

**Players:** [Requirements](#requirements) · [Install](#install) · [Use](#use) · [Troubleshooting](#troubleshooting)
· [Config files & console variables](#config-files-and-console-variables)

**Developers:** [Building](#building-from-source) · [How it works](#how-it-works)

## Requirements

- An **NVIDIA RTX** graphics card (RTX 20 series or newer) with a recent driver.
- Kingdom Come: Deliverance, **Steam version 1.9.8**. Other versions (GOG, older patches) are
  untested. The mod's menu warns you if it doesn't recognise your game version.
- Not tested together with ReShade, ENB or other graphics injectors.

## Install

1. **[Download the latest release](../../releases/latest).** Under *Assets*, get `kcd-clarity-<version>.zip`.
   (The green *Code* button downloads the source code, which you don't need.)
2. Open the game folder. In Steam: right-click the game > *Manage* > *Browse local files*. Then open `Bin\Win64`.
3. Copy `dinput8.dll` and `nvngx_dlss.dll` from the zip into `Bin\Win64`.
4. Start the game. In *Options > Video*, set **Anti-aliasing** to **SMAA 2TX**. DLSS takes the place of that setting,
   so with any other anti-aliasing option the mod does nothing.
5. In game, press **F8**, select **DLSS**, and pick a quality level.

**Other mods:** this mod is loaded as `dinput8.dll`. Other mods that use their own `dinput8.dll` (Ultimate ASI Loader,
ASI plugins, other DLSS mods) can't be installed at the same time. Copying one over the other replaces it.

**Uninstall:** delete `dinput8.dll` and `nvngx_dlss.dll` from `Bin\Win64`. You can also delete `kcd_clarity.ini` and
`kcd_clarity.log` there.

## Use

Press **F8** in game to open the settings, and again to close them. While the menu is open, the mouse controls the
menu instead of the game.

| setting | what it does |
|---|---|
| Engine TAA / DLSS | switch between the game's own anti-aliasing and DLSS, live |
| Quality | the resolution the game renders at before DLSS: DLAA 100%, Quality 66.7%, Balanced 58%, Performance 50%, Ultra Performance 33%. Lower = more FPS, softer image |
| Model preset | which DLSS model to use. *Default* lets the driver choose; K is NVIDIA's recommended model for quality |
| Texture mip bias | keeps textures sharp at lower quality levels. The offset slider fine-tunes it (negative = sharper, can shimmer) |
| Shadow noise | two fixes for shadow flicker with DLSS. Leave them on unless shadows look wrong |
| FPS counter | small FPS readout in the corner |

Your settings are saved in `Bin\Win64\kcd_clarity.ini`. To use different keys, change `toggle_key` (F8) or
`debug_key` (F3) under `[ui]` there, using [Windows virtual-key codes](https://learn.microsoft.com/windows/win32/inputdev/virtual-key-codes)
in decimal (F8 = 119). Leave the `[debug]` section at its defaults.

**F3** opens a debug window. You only need it for bug reports.

## Troubleshooting

| problem | what to check |
|---|---|
| F8 does nothing | the two DLLs must be in `Bin\Win64`, not the game's main folder. If `kcd_clarity.log` doesn't appear there after starting the game, the mod isn't loading; check for another mod's `dinput8.dll` |
| F8 menu: "Set Anti-aliasing to SMAA 2TX" | change it in *Options > Video* |
| F8 menu: "DLSS is not available" | needs an RTX card, an up-to-date NVIDIA driver, and `nvngx_dlss.dll` next to `dinput8.dll` |
| F8 menu: "engine functions not found" or "Untested game version" | your game version differs from the one the mod supports, usually after a game update. Check for a newer mod release |
| something looks wrong below 100% | try DLAA. If it's fine there, it's a bug in the lower-resolution path; please report it |

## Reporting bugs

[Open an issue](../../issues) with:
- your GPU, the quality level, the game's resolution and video settings;
- a screenshot, ideally of the same spot with *Engine TAA* and with *DLSS*;
- `Bin\Win64\kcd_clarity.log` from that session (attach the file).

## Config files and console variables

The **[config guide](https://eonflux7.github.io/kcd1-clarity/cvars/)** builds a `user.cfg` / `autoexec.cfg` for
your PC, checks any config line by line, and lists every console variable with what it really does. The research
behind it is in [docs/cvars](docs/cvars/README.md), which explains how the game loads `user.cfg` and `autoexec.cfg`, and which tweaks there
actually do something. Many popular lines are refused or overwritten by the game. It includes a script that checks
your own config. You don't need any of it to use this mod.

---

## Building from source

Needs Windows and Visual Studio 2022 (or the Build Tools) with the *Desktop development with C++* workload; CMake and
Ninja come with it. Then run `build.cmd`. On the first build, CMake downloads Dear ImGui, MinHook and the needed DLSS
SDK files and checks them against pinned SHA-256 hashes ([THIRD_PARTY.md](THIRD_PARTY.md)).

- `build.cmd` builds `build\dinput8.dll` (RelWithDebInfo, static CRT; the PDB is `build\dinput8.pdb`).
- `build.cmd dist` also writes `dist\kcd-clarity-<version>.zip`, the release package, including
  [release/README.txt](release/README.txt). The version comes from `project()` in `CMakeLists.txt`.
- To test, copy `build\dinput8.dll` and `build\ngx\rel\nvngx_dlss.dll` into the game's `Bin\Win64`.
  `build\ngx\dev\nvngx_dlss.dll` is NVIDIA's development runtime, which can show NVIDIA's DLSS debug overlay. Use it
  for testing only, never in a release.
- From WSL, run `build.cmd` through a drive-letter path (e.g. `Z:\...`); a `\\wsl.localhost\` path fails to link.
  `./deploy.sh` copies the build (with the dev runtime) into the game, `--remove` takes it out again, and `KCD_BIN`
  overrides the game path.

Debugging: the log (`kcd_clarity.log`) and the F3 window come first. `[debug] d3d_debug=1` in the ini turns on the
D3D11 debug layer, with its messages in the log. For RenderDoc captures, tick *Skip NGX* in F3, since NGX doesn't run
under RenderDoc.

## How it works

- **Loading.** `WHGame.dll` imports `DirectInput8Create`, and Windows looks in `Bin\Win64` before System32. The mod
  forwards the dinput8 exports to the system DLL (`src/core/proxy_*`). An init thread waits until the game loads
  `d3d11.dll`, then installs its hooks (MinHook).
- **Engine functions.** `src/generated/kcd_symbols.h` lists the engine functions and data the mod uses, with byte
  patterns and RVAs for the supported build. `src/game/symbols.cpp` resolves them at startup. Data symbols (RVA only)
  are used only on that build.
- **DLSS.** `src/hooks/d3d11_hooks.cpp` recognises the engine's TAA resolve shader (PostAA, SMAA 2TX) by its
  reflection. When that shader draws, `src/renderer/taa_dlss.cpp` builds full-screen motion vectors the way the TAA
  shader does, runs DLSS (`src/upscaler/dlss.cpp`), and writes the result to the TAA output.
- **Upscaling.** Below 100%, the engine's own viewport downscale renders the scene into the top-left sub-rect of each
  full-size target. Several passes don't handle that correctly. `src/renderer/screen_size_fix.cpp` and
  `src/game/renderer.cpp` fix them by patching constants and UVs per draw. `src/renderer/canary.cpp` is a debug tool
  that finds passes reading outside the sub-rect.
- **Image quality.** Motion vectors are promoted to 16 bits (`velocity_format`). Object velocities left on
  now-occluded pixels are dropped (`velocity_mask`). Texture LOD is biased for the render scale (`mip_bias`), and the
  shadow filter noise is made temporal (`shadow_noise`).
- **UI.** Dear ImGui, drawn from a `Present` hook (`src/hooks/present.cpp`, `src/ui/`).

The symbols and engine notes come from a separate reverse-engineering workspace that isn't published. Source comments
that cite `kcd-re db/notes/*.md` refer to it. Porting to another game build means finding the symbols in
`kcd_symbols.h` again, starting from their signatures.

## License

MIT for the source in this repository; see [LICENSE](LICENSE). Third-party components keep their own licenses
([THIRD_PARTY.md](THIRD_PARTY.md)). NVIDIA, RTX and DLSS are trademarks of NVIDIA Corporation.
