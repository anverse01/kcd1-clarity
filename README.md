# KCD Clarity

DLSS for Kingdom Come: Deliverance (Steam, DX11). Work in progress: the current build is an engine probe, with no upscaling yet.

- Build: `build.cmd` on Windows (MSVC + Ninja, VS with C++ x64 tools). From WSL: `cmd.exe /c 'Z:\home\eon\projects\kcd\kcd-clarity\build.cmd'`.
- Deploy: `./deploy.sh` copies `dinput8.dll` (+ dev `nvngx_dlss.dll`) into `Bin/Win64`; `./deploy.sh --remove` undoes it.
- In game: **F3** opens the debug/probe window, **F8** the menu. Log: `Bin/Win64/kcd_clarity.log`.
- Engine addresses come from `../kcd-re` (`kre db export` writes `src/generated/kcd_symbols.h`).
