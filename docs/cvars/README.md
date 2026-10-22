# Console variables and config files in KCD

Research done while building KCD1 Clarity: how Kingdom Come: Deliverance (Steam 1.9.8) loads `user.cfg` and
`autoexec.cfg`, which cvars a config file can actually change, and which values the game overwrites afterwards. It
comes from static analysis of `WHGame.dll`. Nothing in it has been benchmarked.

**Use it in your browser:** [config generator, config checker and cvar database](https://eonflux7.github.io/kcd1-clarity/cvars/).

## Short version

- **`user.cfg` values for anything in the video options get replaced.** The game applies your saved menu settings
  (resolution, vsync, FPS cap, FOV, distance sliders and the quality levels with their 209 cvars) after `user.cfg`
  has been read.
- **About 1700 of the 4007 cvars are cheat or const.** Config files can't change them unless the game runs with
  `-devmode`. That includes favourites like `r_MultiThreaded`, `r_DepthOfField` and `e_PreloadMaterials`.
- **`autoexec.cfg` probably runs after the menu settings** and so wins, but this is **unverified**. Even if it does,
  the value is lost again whenever you change that menu option. A few thread/worker cvars are read at startup, before
  autoexec runs, and only work from `user.cfg`.
- **Parsing traps:** comments must *start* the line (`;` or `--`). In `user.cfg` every line needs `=`, and `bind`
  only works in autoexec. A fraction in an int cvar truncates (`r_HDRBloom = 0.4` turns bloom off). A file saved as
  `user.cfg.txt` is never loaded.

## Check your own config

1. Install [Python 3](https://www.python.org/downloads/) (on Windows, tick *Add python.exe to PATH*).
2. Download this repository: green *Code* button > *Download ZIP*, then extract it.
3. Open a terminal in the extracted `docs\cvars` folder (in Explorer: type `cmd` in the address bar) and run, with
   your own path:
   ```
   py tools\cfg_audit.py "C:\Program Files (x86)\Steam\steamapps\common\KingdomComeDeliverance\user.cfg"
   ```
   (Linux/macOS: `python3 tools/cfg_audit.py path/to/user.cfg`.) You can pass `user.cfg` and `autoexec.cfg`
   together. Add `--md report.md` to also get a Markdown table.

Each line gets a verdict, such as `OK`, `OVERRIDDEN` (the video options replace it) or `BLOCKED` (needs `-devmode`),
with the reason next to it. A legend is printed at the end. The rules behind the verdicts are in
[cfg-loading.md](cfg-loading.md).

## Files

| file | what |
|---|---|
| [cfg-loading.md](cfg-loading.md) | load order, parsing rules, cheat/const checks, who overwrites what, dead cvars. Engine addresses for every claim |
| [community-cfg-audit.md](community-cfg-audit.md) | 9 popular community configs checked line by line, plus the full quality-preset tables (what each menu level sets) |
| [perf-cvars.md](perf-cvars.md) | cvars ranked by likely stutter / GPU / CPU impact, with where each one has to go. **Not benchmarked** |
| [data/cvars-WHGame-1.9.8.jsonl](data/cvars-WHGame-1.9.8.jsonl) | every cvar registration: name, type, default, flags, const, help text |
| [data/cvargroups-WHGame-1.9.8.json](data/cvargroups-WHGame-1.9.8.json) | what each quality preset group sets, per level (Low, Medium, High, Very High, 7) |
| [tools/cfg_audit.py](tools/cfg_audit.py) | checks a config (see above). Python 3, no extra packages |
| [index.html](index.html), [site/](site/) | the web page; `site/data.json` is built by `tools/build_site_data.py` from `data/` |
| [tools/cvar_registry.py](tools/cvar_registry.py) | IDA script that produced the cvar data (only needed for another game build) |

## Open questions

- Does `autoexec.cfg` run before or after the profile is loaded? There's a 5-minute test recipe in
  [cfg-loading.md](cfg-loading.md) section 4. If you run it, please [open an issue](../../../../issues) with your `kcd.log`.
- Real frame-time numbers for the [perf-cvars.md](perf-cvars.md) candidates.
