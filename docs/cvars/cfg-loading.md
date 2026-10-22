# CVars, user.cfg / autoexec.cfg loading, and what actually sticks

Binary: Steam `WHGame.dll` 1.9.8 (sha256 `cf9f6dc3…`). Addresses (`sub_180…`, `0x…`) are from that build; bare hex
offsets are RVAs relative to the WHGame.dll image base. Everything here is static analysis unless marked otherwise.
The one open runtime question is in section 4.

Data:
- [`data/cvars-WHGame-1.9.8.jsonl`](data/cvars-WHGame-1.9.8.jsonl): **4045 registrations / 4007 unique cvars**, with
  type, registration default, flags, allowModify, help text and the registering function, from
  [`tools/cvar_registry.py`](tools/cvar_registry.py) (IDA). A `null` field is a value the linear scan couldn't resolve,
  not an empty one.
- Not covered by the registry scan: CVarGroups (`sys_spec_*`, created from `Config/CVarGroups/*.cfg` at runtime,
  "creating CVarGroups from directory" in 0xe955b4) and the ~20 `sys_Pak*` vars registered through the CSystem helper
  `sub_180EB0938`.
- [`data/cvargroups-WHGame-1.9.8.json`](data/cvargroups-WHGame-1.9.8.json): the cvars and
  per-level values of each `Config/CVarGroups/*.cfg` in `Engine/engine.pak` (the pak is a zip), with a level's
  missing entries filled from `[default]`. The same values are tabulated in
  [`community-cfg-audit.md`](community-cfg-audit.md#quality-preset-tables-configcvargroups-in-enginepak).
- [`tools/cfg_audit.py`](tools/cfg_audit.py) `<cfg...>` applies every rule below to any user.cfg / autoexec.cfg
  (Python 3, no dependencies). Results for 9 community configs are in [`community-cfg-audit.md`](community-cfg-audit.md).

## 1. Load order

| step | where | file / source | how values are set |
|---|---|---|---|
| 1 | CSystem::Init, `sub_180EA3AA8` | `system.cfg` (must exist, else MessageBox + exit) | LoadConfiguration -> `CXConsole::LoadConfigVar` |
| 2 | CSystem::Init 0xe9ef63 -> `sub_180EA3CB0` | `tester.cfg` (only when vfn 0x608 is false), mods' `mod.cfg` via `wh::I_Mod` lambda, `developer.cfg`, **`user.cfg`** | same |
| 3 | CSystem::Init, after 2 | job manager (`sys_job_system_max_worker` read at 0xe9efca), stream engine thread (`sys_streaming_CPU`, 0xe912f4), thread task manager `sub_180EA2FE8` (called 0xe9fda2: `sys_main_CPU`, `sys_TaskThread0..3_CPU`) | reads cvar values once |
| 4 | after system init (CryAction / game) | player profile `Saved Games/kingdomcome/profiles/<p>/attributes.xml` -> `wh::guimodule::CUISettings` load `sub_181150D40` | see section 3 |
| 5 | `wh::game::C_GameStartup::Run` (vtable slot 8, `sub_180DA6AC0`) | `ExecuteString("exec autoexec.cfg")` | console command path |

Search paths: LoadConfiguration (`sub_180EA7018`, systemcfg.cpp) tries `./<file>`, then `<file>`, then `config/<file>`.
`exec` (`sub_180E123FC`, consolebatchfile.cpp) tries `game/`, `config/`, `game/config/`, then `./`. Both live in the
game root next to `system.cfg`. **A file named `user.cfg.txt` is never loaded** (one Nexus archive ships exactly that).

## 2. Parsing (verified in both loaders)

- Lines are split on CR/LF and trimmed. A line is skipped if it's empty, **starts with** `;`, or **starts with** `--`.
  `--` later in a line is *not* a comment.
- user.cfg: needs `=`, otherwise "invalid configuration line" is logged and the line is ignored. `#comments`, `bind`,
  `<!-- -->` and prose lines in user.cfg all fall in this bucket. The value is the trimmed text after `=`, or the
  content of `"..."`. A trailing `-- comment` / `— comment` stays in the value.
- autoexec.cfg: each line goes through `ExecuteString(line)` (vfn 0x118, `sub_180EC6920` -> `sub_180EC6A7C`). It
  splits commands on `;`, accepts `name=value` or `name value`, strips ` \t\r\n"'` from the value, and can run
  commands (`bind` works here and doesn't work in user.cfg).
- Int cvars parse with `strtoi64` (`sub_1806F6844`) and float cvars with atof, so a trailing comment after a number is
  harmless. **Fractions into int cvars truncate** (e.g. `r_HDRBloom = 0.4` -> 0, i.e. bloom off). An **empty value**
  (`sys_budget_sysmem=`) sets 0. String cvars keep the comment.
- `wait_seconds`/`wait_frames` in autoexec don't defer anything. The top-level `exec autoexec.cfg` runs non-deferred,
  and deferral only happens when the console's +0x128 flag is set (`sub_180EC6920`).

## 3. Who is allowed to set what

Flag values (CE3.8 IConsole.h, matching the checks below): 0x2 VF_CHEAT, 0x4 VF_DEPRECATED, 0x100 VF_DUMPTODISK,
0x800 VF_READONLY, 0x1000 REQUIRE_LEVEL_RELOAD, 0x2000 REQUIRE_APP_RESTART, 0x20000 VF_WASINCONFIG, 0x80000
VF_RESTRICTEDMODE, 0x100000 VF_INVISIBLE, 0x800000 VF_CONST_CVAR, 0x1000000 CHEAT_ALWAYS_CHECK, 0x2000000
CHEAT_NOCHECK, 0x20000000 VF_RENDERER_CVAR. 0x40000000 is a KCD flag that is refused even in devmode.

- `CXConsole::LoadConfigVar` (vfn 0x1F8, `sub_180ECC1FC`, used by system/user.cfg and by CVarGroup apply), and
  `OnBeforeVarChange` (vfn 0x220, `sub_18055559C`, called by every `ICVar::Set`, so the autoexec/console path too). Both
  **refuse** the change and log `[CVARS]: [%s] variable ... Marked as [VF_CHEAT]...` (`sub_180ECD60C`) when:
  `IsConstCVar() || flags & (0x2|0x1000000|0x2000000|0x800)`, unless `ISystem::IsDevMode()` (vfn 0x360, `-devmode`
  on the command line, `sub_180EAB250`). A cvar with flag 0x40000000 is refused even in devmode.
  - Bypass: console byte +0x1DE. It's set while a CVarGroup applies its children (`sub_180EC1A18`,
    `sub_180ED7358`), so the presets can set cheat/const cvars and configs can't.
- **Const = registered with `allowModify=false`.** `Register(int/float/string*)` (`sub_180ED3498`/`ED364C`/`ED380C`)
  OR in 0x800000 when the last arg is 0, and `IsConstCVar` (`sub_1805556D0`) tests bit 23. 441 registrations in retail
  are const, including `r_MultiThreaded`, `r_GeomInstancing`, `r_DepthOfField`, `r_TexturesStreaming`,
  `e_PreloadMaterials`, `ca_thread0/1Affinity`, `e_StatObjMergeUseThread`, `e_TerrainDetailMaterialsViewDistZ`.
  Example: r_DepthOfField registration at 0x1fb539b passes `[rsp+38h] = r10b = 0`.
- Counts over the registry: 1241 VF_CHEAT, 252 CHEAT_NOCHECK, 10 CHEAT_ALWAYS_CHECK, 7 READONLY, 441 const. **Roughly
  1700 of the ~4000 cvars can't be changed by any cfg file without `-devmode`.**
- Cvars that aren't registered yet when the cfg is read (game-module cvars registered later) are queued and applied at
  registration by `sub_180ED41F8`, with the same checks.
- `con_restricted` (VF_RESTRICTEDMODE, var 0x3785888): it's forced to 0 in devmode (0xeca13a) and nonzero otherwise.
  When nonzero, *typed* console input (and tab-completion) only reaches cvars flagged 0x80000. It doesn't affect cfg
  files and has no perf impact. Setting it to 0 in a cfg works and only unlocks typing ordinary cvars in the console.

## 4. Who overwrites your values afterwards

**CVarGroups (`sys_spec_*`).** `CXConsoleVariableCVarGroup::Set(int)` (`sub_180ED7358`) re-applies the whole group
whenever the new level differs **or any child cvar differs from the group table** (`sub_180ED8B28` ->
`sub_180EDC65C`). The apply goes through `LoadConfigVar` with the cheat bypass and **does not** honour VF_WASINCONFIG.
Only the spec-file sink `sub_180EA5DAC` ("NOT VF_WASINCONFIG Ignoring cvar") skips user-set values, and that sink is
only used for `LowSpec.cfg`/`HighSpec.cfg`-style files (`sub_180EA62A4`), not for the groups.

**The profile.** `CUISettings` load `sub_181150D40` reads `attributes.xml` and then:
- `sub_18115737C`: resolution/fullscreen (`r_Width`, `r_Height`, `r_Fullscreen*`), `r_VSync`, `sys_MaxFPS` (menu only
  has 0 / 30 / 60), gamma. It also **writes the menu FOV straight into `cl_fov`'s storage** (game cvars +0xF8, the
  same member `cl_fov` is registered on, ef14f5).
- `sub_1811492C4`: `sys_spec_ObjectDetail, GameEffects, light, Particles, Physics, PostProcessing, Shading, Shadows,
  Texture, TextureResolution (= Texture level), Water, VolumetricEffects, Vegetation`, `r_MotionBlur`,
  `r_AntialiasingMode`. So every child in those 13 groups gets reset to the menu's level. That covers 209 cvars, 197 of
  them otherwise settable (shadows, SSDO/SVOTI, LOD, vegetation, texture pool, anisotropy...).
- `sub_181157130`: the three distance sliders -> `e_ViewDistRatio{,Detail,Lights,Portals,Vegetation}`, `e_LodRatio`,
  `e_MergedMeshesViewDistRatio`.
- The same functions run again whenever the user changes those options in the menu (the callers 0x11541b8 and
  0x1154acc are the menu apply paths).

Consequence for **user.cfg**: it's parsed during CSystem::Init, long before the profile exists. So **any user.cfg line
for a cvar in one of those 13 groups, or in the menu list above, is silently replaced** by the menu value (certain).

Consequence for **autoexec.cfg**: **unverified.** It runs at the start of `C_GameStartup::Run`. The profile is loaded
by CryAction's `CPlayerProfileImplFSDir` (vtable 0x23c7690), most likely during game Init, which would make autoexec
the last writer. But that's not proven, and the values are lost anyway the next time the matching menu setting is
applied. This is probably where the community's "put it in autoexec, not user.cfg" folklore comes from.
To verify (not run yet): append to user.cfg `log_Verbosity=3`, `log_WriteToFileVerbosity=3`,
`e_ShadowsMaxTexRes=768`, `cl_fov=101`, `sys_MaxFPS=37`, `e_ShadowsClouds=0` (control, not in any group), and
`r_DepthOfField=0` (const, should log a refusal). Put just the bare names in autoexec.cfg (one per line, which prints
the values), start without `-devmode`, and read `kcd.log`. If the sentinels are already reset when autoexec prints,
the profile came first and autoexec wins.

**Init-time readers.** These are read once in CSystem::Init right after user.cfg, so they work from user.cfg and are
**too late in autoexec.cfg**: `sys_job_system_max_worker` (CThreadBackEnd::Init `sub_180E3AC78`: value, 0 -> logical
CPU count `SYSTEM_INFO.dwNumberOfProcessors`, clamped to it; default 8), `sys_main_CPU`, `sys_TaskThread0..3_CPU`,
`sys_streaming_CPU` (affinity mask `1 << n`).

## 5. Registered but dead (no effect whatever you set)

| cvar | evidence |
|---|---|
| `sys_preload` | ICVar* stored at CSystem+0xAD0 (0xe99daf); nothing reads +0xAD0 except ctor/dtor |
| `r_WaterUpdateThread` | var 0x378f304: only xref is the registration |
| `sys_flash_address_space` | var 0x2b7bf70: only xref is the registration |
| `sys_TaskThread4_CPU`, `sys_TaskThread5_CPU` | `sub_180EA2FE8` reads `[sys+i*8+0x2888]` only while `i < 4` (0xea319c) |
| `sys_PakStreamCache` | gates adding `Levels/<lvl>/_cache/` to the stream cache (`sub_180E72A3C`); no `_cache` exists in `Data/Levels/*` or inside the level paks |
| `sys_budget_sysmem`, `sys_budget_videomem` | members of `CBudgetingSystem` (debug overlay enabled by `sys_enable_budgetmonitoring`), registered in `sub_180E0C11C`; they don't size or limit anything |
| `e_ParticlesThread` | the reads found (0x4ccc7c, 0x4cda32) only test it against 0; 11 behaves like the default 1 |
| `r_MultiGPU` | `sub_1803BE704`: picks the AFR GPU count; nothing on a single GPU |
| `wh_cs_Player*` lock tunables | registered with VF_DEPRECATED (0x4) by Warhorse; probably leftovers (unverified) |

Not cvars at all (silently ignored): `WH_AI_CombatMove_TacticalRangeGain/Surround` (AI data strings,
0x1488b08), `sys_spec_twtoggle` (only exists with a specific mod), `pl_movement.*` (exists, since the old regex just
missed it).

## 6. Perf-relevant knobs that are real

The engine's own cost knobs are the CVarGroup children (per-level tables in [`community-cfg-audit.md`](community-cfg-audit.md)). Because of
section 4, the reliable ways to change them are: pick the menu level; or set them in autoexec (pending the section 4
check, and re-applied after menu changes); or edit the group files themselves (a `Config/CVarGroups/*.cfg` override,
untested here).
Real, settable and outside any group (so user.cfg values stick): `r_TexturesStreamingMipBias` (negative = higher-res
mips = more VRAM/IO, so more streaming hitches, not fewer), `r_TexturesStreamingMaxRequestedMB`,
`r_TexturesStreamingResidencyTime/Throttle`, `e_StreamCgfPoolSize` (`system.cfg` already sets 512; the configs'
256/512 are no gain or a reduction), `r_ShadersAsyncCompiling` (`system.cfg` sets 3 = precache mode; the configs' 2 is
a downgrade), `e_VolumetricFog`, `r_VolumetricFog*`, `e_svoTI_ResScaleBase` (GI resolution; 0/1 = more expensive than
the default 2), `e_svoTI_*` amplifiers (looks only), `e_MergedMeshesActiveDist`, `e_ShadowsClouds`,
`e_UberlodUpdateBudget`, `r_DrawNearFoV`.
`r_BatchType` is read per frame (`sub_1803DF7B4`, 0x3dfc14) and changes a per-batch heuristic. 1 is the default and
2 = auto. No measured effect yet.
Nothing here has been benchmarked. The static analysis says which lines *can* change anything, not by how many fps.
