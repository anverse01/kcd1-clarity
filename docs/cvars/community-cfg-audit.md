# Audit of 9 community user.cfg / autoexec.cfg configs

Generated with [`tools/cfg_audit.py`](tools/cfg_audit.py) from the rules in [`cfg-loading.md`](cfg-loading.md).
Sources: 9 popular performance/quality configs from Nexus Mods, downloaded October 2026. They're labelled A–I here;
the point is what the engine does with these lines, not who wrote them. Run the tool on any config to get the same
verdicts.

Verdicts:
- **OK**: the value is applied and nothing overwrites it later.
- **OK_INIT**: read once at startup; works from user.cfg.
- **TOO_LATE**: an init-time cvar placed in autoexec, so it's read before autoexec runs.
- **OVERRIDDEN**: user.cfg value replaced by the profile (menu settings / quality presets) at load.
- **AT_RISK**: same cvar placed in autoexec; it survives only if autoexec runs after the profile (unverified) and is
  lost again whenever that menu setting is applied.
- **BLOCKED**: cheat/const/read-only; refused unless the game runs with `-devmode`.
- **DEAD**: registered but has no effect.
- **MISSING**: not a cvar in this build.
- **IGNORED**: not a `key=value` line.
- **GROUP**: a non-menu CVarGroup (sys_spec_Sound).

## Per file
```
config  file                OK  OK_INIT    GROUP OVERRIDD  AT_RISK  BLOCKED     DEAD TOO_LATE  MISSING  IGNORED
A       user.cfg             9        2        0       19        0        3        2        0        0        0
B       autoexec.cfg        20        0        1        0       32        7        4        1        0        0
C       autoexec.cfg        31        0        1        0       45       14        8        7        0        0
D       user.cfg            37        7        0       41        0       20        8        0        0       11
E       user.cfg.txt         6        0        0        4        0        0        5        0        0        0
F       autoexec.cfg        17        0        0        0       23        1        4        1        0        0
G       user.cfg            31        2        0       20        0        8        4        0        3        1
H       user.cfg            10        0        1       25        0        0        2        0        0        0
I       user.cfg             2        0        0        2        0        0        0        0        0        0
```
Notes:
- E ships `user.cfg.txt`. The engine only opens `user.cfg`, so nothing in it loads unless renamed.
- F ships `sys_budget_sysmem=`, `sys_budget_videomem=` and `r_TexturesStreamPoolSize=` empty "for the
  user to fill". Left empty they set 0, though the pool size gets re-applied by the texture preset and the budgets are
  dead anyway.
- D: its 11 `#Section` lines and `bind p #NightVisionToggle()` are invalid in user.cfg
  (`bind` only works in autoexec).
- G and D: `r_HDRBloom = 0.4` is an int cvar, so it truncates to 0 and turns bloom
  off. `r_AntialiasingTAASharpening`, `r_Sharpening` etc. are floats and fine.
- G's instructions claim that a yellow highlight in the console proves the file ran. That only shows a cvar *exists*, not
  that the value stuck.

## Every cvar used, deduplicated
| cvar | used in | verdict in user.cfg | verdict in autoexec.cfg | why |
|---|---|---|---|---|
| `ai_NavigationSystemMT` | 3 | BLOCKED | BLOCKED | cheat: refused unless the game runs with -devmode |
| `ca_thread0Affinity` | 2 | BLOCKED | BLOCKED | const: refused unless the game runs with -devmode |
| `ca_thread1Affinity` | 2 | BLOCKED | BLOCKED | const: refused unless the game runs with -devmode |
| `e_AutoPrecacheCameraJumpDist` | 3 | BLOCKED | BLOCKED | cheat/const: refused unless the game runs with -devmode |
| `e_AutoPrecacheCgf` | 3 | BLOCKED | BLOCKED | cheat/const: refused unless the game runs with -devmode |
| `e_AutoPrecacheTerrainAndProcVeget` | 3 | BLOCKED | BLOCKED | cheat: refused unless the game runs with -devmode |
| `e_AutoPrecacheTexturesAndShaders` | 3 | BLOCKED | BLOCKED | cheat/const: refused unless the game runs with -devmode |
| `e_DecalsMerge` | 1 | BLOCKED |  | const: refused unless the game runs with -devmode |
| `e_PrecacheLevel` | 1 | BLOCKED |  | const: refused unless the game runs with -devmode |
| `e_PreloadMaterials` | 2 | BLOCKED |  | const: refused unless the game runs with -devmode |
| `e_StatObjMergeUseThread` | 2 | BLOCKED | BLOCKED | const: refused unless the game runs with -devmode |
| `e_StreamInstancesMinLoadedNodes` | 1 | BLOCKED |  | cheat: refused unless the game runs with -devmode |
| `e_TerrainDetailMaterialsViewDistZ` | 4 | BLOCKED | BLOCKED | const: refused unless the game runs with -devmode |
| `g_enableMergedMeshRuntimeAreas` | 1 | BLOCKED |  | cheat: refused unless the game runs with -devmode |
| `r_CloudsUpdateAlways` | 1 | BLOCKED |  | const: refused unless the game runs with -devmode |
| `r_DeferredShadingSortLights` | 3 | BLOCKED | BLOCKED | cheat: refused unless the game runs with -devmode |
| `r_DepthOfField` | 2 | BLOCKED | BLOCKED | const: refused unless the game runs with -devmode |
| `r_GeomInstancing` | 2 | BLOCKED | BLOCKED | const: refused unless the game runs with -devmode |
| `r_multithreaded` | 5 | BLOCKED | BLOCKED | const: refused unless the game runs with -devmode |
| `r_TexturesStreaming` | 1 |  | BLOCKED | const: refused unless the game runs with -devmode |
| `r_TexturesStreamingResidencyEnabled` | 1 | BLOCKED |  | const: refused unless the game runs with -devmode |
| `wh_cs_AutomationAction_HuntAttackTimeLimit` | 1 | BLOCKED |  | cheat: refused unless the game runs with -devmode |
| `wh_cs_TimeWarpDodgeFadeSpeedForOpp` | 1 | BLOCKED |  | cheat: refused unless the game runs with -devmode |
| `wh_cs_TimeWarpDodgeFadeSpeedForPlayer` | 1 | BLOCKED |  | cheat: refused unless the game runs with -devmode |
| `wh_cs_TimeWarpPBFadeSpeedForOpp` | 1 | BLOCKED |  | cheat: refused unless the game runs with -devmode |
| `wh_cs_TimeWarpPBFadeSpeedForPlayer` | 1 | BLOCKED |  | cheat: refused unless the game runs with -devmode |
| `wh_ui_FaderFadeInDefaultDuration` | 1 | BLOCKED |  | cheat: refused unless the game runs with -devmode |
| `wh_ui_FaderFadeOutDefaultDuration` | 1 | BLOCKED |  | cheat: refused unless the game runs with -devmode |
| `sys_spec_twtoggle` | 1 | MISSING |  | not a cvar in this build; silently ignored |
| `WH_AI_CombatMove_TacticalRangeGain` | 1 | MISSING |  | not a cvar in this build; silently ignored |
| `WH_AI_CombatMove_TacticalSurround` | 1 | MISSING |  | not a cvar in this build; silently ignored |
| `e_ParticlesThread` | 2 | DEAD | DEAD | only compared against 0; any non-zero value equals the default (1) |
| `r_MultiGPU` | 1 | DEAD |  | only selects GPU count for AFR; no effect on a single GPU |
| `r_WaterUpdateThread` | 2 | DEAD | DEAD | backing var has no reader besides its registration |
| `sys_budget_sysmem` | 7 | DEAD | DEAD | threshold for the CBudgetingSystem debug overlay; allocates/limits nothing |
| `sys_budget_videomem` | 7 | DEAD | DEAD | threshold for the CBudgetingSystem debug overlay; allocates/limits nothing |
| `sys_flash_address_space` | 1 | DEAD |  | backing var has no reader besides its registration |
| `sys_PakStreamCache` | 6 | DEAD | DEAD | only adds Levels/<level>/_cache/ to the stream cache; KCD ships no _cache folders |
| `sys_preload` | 7 | DEAD | DEAD | ICVar* stored at CSystem+0xAD0, never read |
| `sys_TaskThread4_CPU` | 2 | DEAD | DEAD | InitThreads only reads sys_TaskThread0..3_CPU (loop is i < 4) |
| `sys_TaskThread5_CPU` | 2 | DEAD | DEAD | InitThreads only reads sys_TaskThread0..3_CPU (loop is i < 4) |
| `sys_job_system_max_worker` | 4 | OK_INIT | TOO_LATE | worker count = value (0 = logical CPU count), clamped to logical CPUs; default 8 / autoexec: worker count = value (0 = logical CPU count), clamped to logical CPUs; default 8; read during init, before autoexec.cfg runs |
| `sys_main_CPU` | 4 | OK_INIT | TOO_LATE | main thread affinity, set at thread-task-manager init / autoexec: main thread affinity, set at thread-task-manager init; read during init, before autoexec.cfg runs |
| `sys_streaming_CPU` | 4 | OK_INIT | TOO_LATE | stream-engine thread affinity mask = 1 << value / autoexec: stream-engine thread affinity mask = 1 << value; read during init, before autoexec.cfg runs |
| `sys_TaskThread0_CPU` | 2 | OK_INIT | TOO_LATE | task thread affinity / autoexec: task thread affinity; read during init, before autoexec.cfg runs |
| `sys_TaskThread1_CPU` | 2 | OK_INIT | TOO_LATE | task thread affinity / autoexec: task thread affinity; read during init, before autoexec.cfg runs |
| `sys_TaskThread2_CPU` | 2 | OK_INIT | TOO_LATE | task thread affinity / autoexec: task thread affinity; read during init, before autoexec.cfg runs |
| `sys_TaskThread3_CPU` | 2 | OK_INIT | TOO_LATE | task thread affinity / autoexec: task thread affinity; read during init, before autoexec.cfg runs |
| `cl_fov` | 4 | OVERRIDDEN | AT_RISK | re-applied by menu: FOV slider (written straight into the cvar's storage) at profile load, after user.cfg / autoexec: re-applied by menu: FOV slider (written straight into the cvar's storage) whenever that setting is (re)applied |
| `e_Clouds` | 1 | OVERRIDDEN |  | re-applied by quality preset VolumetricEffects at profile load, after user.cfg |
| `e_DecalsLifeTimeScale` | 1 | OVERRIDDEN |  | re-applied by quality preset ObjectDetail at profile load, after user.cfg |
| `e_LodFaceAreaTargetSize` | 4 | OVERRIDDEN | AT_RISK | re-applied by quality preset ObjectDetail at profile load, after user.cfg / autoexec: re-applied by quality preset ObjectDetail whenever that setting is (re)applied |
| `e_LodRatio` | 3 | OVERRIDDEN | AT_RISK | re-applied by menu: LodRatio slider, quality preset ObjectDetail at profile load, after user.cfg / autoexec: re-applied by menu: LodRatio slider, quality preset ObjectDetail whenever that setting is (re)applied |
| `e_MergedMeshesInstanceDist` | 4 | OVERRIDDEN | AT_RISK | re-applied by quality preset Vegetation at profile load, after user.cfg / autoexec: re-applied by quality preset Vegetation whenever that setting is (re)applied |
| `e_MergedMeshesLodRatio` | 5 | OVERRIDDEN | AT_RISK | re-applied by quality preset Vegetation at profile load, after user.cfg / autoexec: re-applied by quality preset Vegetation whenever that setting is (re)applied |
| `e_MergedMeshesPool` | 1 | OVERRIDDEN |  | re-applied by quality preset Vegetation at profile load, after user.cfg |
| `e_MergedMeshesViewDistRatio` | 5 | OVERRIDDEN | AT_RISK | re-applied by menu: MergedMeshesRatio slider, quality preset Vegetation at profile load, after user.cfg / autoexec: re-applied by menu: MergedMeshesRatio slider, quality preset Vegetation whenever that setting is (re)applied |
| `e_ObjShadowCastSpec` | 1 | OVERRIDDEN |  | re-applied by quality preset Shadows at profile load, after user.cfg |
| `e_ShadowsCastViewDistRatio` | 5 | OVERRIDDEN | AT_RISK | re-applied by quality preset Shadows at profile load, after user.cfg / autoexec: re-applied by quality preset Shadows whenever that setting is (re)applied |
| `e_ShadowsCastViewDistRatioLights` | 4 | OVERRIDDEN | AT_RISK | re-applied by quality preset Shadows at profile load, after user.cfg / autoexec: re-applied by quality preset Shadows whenever that setting is (re)applied |
| `e_ShadowsCastViewDistRatioMulInvis` | 4 | OVERRIDDEN | AT_RISK | re-applied by quality preset Shadows at profile load, after user.cfg / autoexec: re-applied by quality preset Shadows whenever that setting is (re)applied |
| `e_ShadowsMaxTexRes` | 6 | OVERRIDDEN | AT_RISK | re-applied by quality preset Shadows at profile load, after user.cfg / autoexec: re-applied by quality preset Shadows whenever that setting is (re)applied |
| `e_ShadowsPoolSize` | 4 | OVERRIDDEN | AT_RISK | re-applied by quality preset Shadows at profile load, after user.cfg / autoexec: re-applied by quality preset Shadows whenever that setting is (re)applied |
| `e_ShadowsUpdateViewDistRatio` | 2 | OVERRIDDEN | AT_RISK | re-applied by quality preset Shadows at profile load, after user.cfg / autoexec: re-applied by quality preset Shadows whenever that setting is (re)applied |
| `e_svoTI_ConeMaxLength` | 3 | OVERRIDDEN | AT_RISK | re-applied by quality preset Shading at profile load, after user.cfg / autoexec: re-applied by quality preset Shading whenever that setting is (re)applied |
| `e_svoTI_DiffuseConeWidth` | 3 | OVERRIDDEN | AT_RISK | re-applied by quality preset Shading at profile load, after user.cfg / autoexec: re-applied by quality preset Shading whenever that setting is (re)applied |
| `e_svoTI_LowSpecMode` | 3 | OVERRIDDEN | AT_RISK | re-applied by quality preset Shading at profile load, after user.cfg / autoexec: re-applied by quality preset Shading whenever that setting is (re)applied |
| `e_TerrainAo` | 3 | OVERRIDDEN | AT_RISK | re-applied by quality preset Shading at profile load, after user.cfg; equals the registration default / autoexec: re-applied by quality preset Shading whenever that setting is (re)applied; equals the registration default |
| `e_VegetationUseTerrainColor` | 1 | OVERRIDDEN |  | re-applied by quality preset Shading/Vegetation at profile load, after user.cfg; equals the registration default |
| `e_VegetationUseTerrainColorDistance` | 5 | OVERRIDDEN | AT_RISK | re-applied by quality preset Vegetation at profile load, after user.cfg / autoexec: re-applied by quality preset Vegetation whenever that setting is (re)applied |
| `e_ViewDistRatio` | 1 | OVERRIDDEN |  | re-applied by menu: ViewDistRatio slider, quality preset ObjectDetail at profile load, after user.cfg |
| `e_viewdistratiolights` | 2 | OVERRIDDEN | AT_RISK | re-applied by menu: ViewDistRatio slider, quality preset ObjectDetail at profile load, after user.cfg / autoexec: re-applied by menu: ViewDistRatio slider, quality preset ObjectDetail whenever that setting is (re)applied |
| `e_ViewDistRatioVegetation` | 2 | OVERRIDDEN |  | re-applied by menu: ViewDistRatio slider, quality preset ObjectDetail at profile load, after user.cfg; equals the registration default |
| `r_AntialiasingMode` | 6 | OVERRIDDEN | AT_RISK | re-applied by menu: AA, quality preset PostProcessing at profile load, after user.cfg; equals the registration default / autoexec: re-applied by menu: AA, quality preset PostProcessing whenever that setting is (re)applied |
| `r_Beams` | 3 | OVERRIDDEN | AT_RISK | re-applied by quality preset VolumetricEffects at profile load, after user.cfg / autoexec: re-applied by quality preset VolumetricEffects whenever that setting is (re)applied |
| `r_ChromaticAberration` | 2 | OVERRIDDEN | AT_RISK | re-applied by quality preset PostProcessing at profile load, after user.cfg; equals the registration default / autoexec: re-applied by quality preset PostProcessing whenever that setting is (re)applied; equals the registration default |
| `r_DeferredShading3PL` | 2 | OVERRIDDEN | AT_RISK | re-applied by quality preset Shading at profile load, after user.cfg / autoexec: re-applied by quality preset Shading whenever that setting is (re)applied |
| `r_DeferredShadingAreaLights` | 5 | OVERRIDDEN | AT_RISK | re-applied by quality preset Light at profile load, after user.cfg; equals the registration default / autoexec: re-applied by quality preset Light whenever that setting is (re)applied; equals the registration default |
| `r_DeferredShadingFilterGBuffer` | 3 | OVERRIDDEN | AT_RISK | re-applied by quality preset Shading at profile load, after user.cfg / autoexec: re-applied by quality preset Shading whenever that setting is (re)applied |
| `r_DynTexAtlasCloudsMaxSize` | 1 | OVERRIDDEN |  | re-applied by quality preset Texture at profile load, after user.cfg |
| `r_FogShadows` | 6 | OVERRIDDEN | AT_RISK | re-applied by quality preset Shadows at profile load, after user.cfg; equals the registration default / autoexec: re-applied by quality preset Shadows whenever that setting is (re)applied |
| `r_fogShadowsWater` | 3 | OVERRIDDEN | AT_RISK | re-applied by quality preset Shadows at profile load, after user.cfg / autoexec: re-applied by quality preset Shadows whenever that setting is (re)applied |
| `r_motionblur` | 1 | OVERRIDDEN |  | re-applied by menu: motion blur, quality preset PostProcessing at profile load, after user.cfg |
| `r_MotionBlurMaxViewDist` | 2 | OVERRIDDEN | AT_RISK | re-applied by quality preset PostProcessing at profile load, after user.cfg; equals the registration default / autoexec: re-applied by quality preset PostProcessing whenever that setting is (re)applied; equals the registration default |
| `r_MotionBlurQuality` | 2 | OVERRIDDEN | AT_RISK | re-applied by quality preset PostProcessing at profile load, after user.cfg / autoexec: re-applied by quality preset PostProcessing whenever that setting is (re)applied |
| `r_RainMaxViewDist_Deferred` | 2 | OVERRIDDEN |  | re-applied by quality preset PostProcessing at profile load, after user.cfg |
| `r_ShadowCastingLightsMaxCount` | 4 | OVERRIDDEN | AT_RISK | re-applied by quality preset Shadows at profile load, after user.cfg; equals the registration default / autoexec: re-applied by quality preset Shadows whenever that setting is (re)applied; equals the registration default |
| `r_Sharpening` | 4 | OVERRIDDEN | AT_RISK | re-applied by quality preset PostProcessing at profile load, after user.cfg / autoexec: re-applied by quality preset PostProcessing whenever that setting is (re)applied |
| `r_ssdo` | 2 | OVERRIDDEN |  | re-applied by quality preset Shading at profile load, after user.cfg; equals the registration default |
| `r_ssdoAmountAmbient` | 4 | OVERRIDDEN | AT_RISK | re-applied by quality preset Shading at profile load, after user.cfg / autoexec: re-applied by quality preset Shading whenever that setting is (re)applied |
| `r_ssdoAmountDirect` | 4 | OVERRIDDEN | AT_RISK | re-applied by quality preset Shading at profile load, after user.cfg; equals the registration default / autoexec: re-applied by quality preset Shading whenever that setting is (re)applied; equals the registration default |
| `r_ssdoAmountReflection` | 4 | OVERRIDDEN | AT_RISK | re-applied by quality preset Shading at profile load, after user.cfg / autoexec: re-applied by quality preset Shading whenever that setting is (re)applied |
| `r_ssdoHalfRes` | 4 | OVERRIDDEN | AT_RISK | re-applied by quality preset Shading at profile load, after user.cfg; equals the registration default / autoexec: re-applied by quality preset Shading whenever that setting is (re)applied; equals the registration default |
| `r_ssdoRadius` | 4 | OVERRIDDEN | AT_RISK | re-applied by quality preset Shading at profile load, after user.cfg / autoexec: re-applied by quality preset Shading whenever that setting is (re)applied |
| `r_ssdoRadiusMax` | 4 | OVERRIDDEN | AT_RISK | re-applied by quality preset Shading at profile load, after user.cfg / autoexec: re-applied by quality preset Shading whenever that setting is (re)applied |
| `r_ssdoRadiusMin` | 4 | OVERRIDDEN | AT_RISK | re-applied by quality preset Shading at profile load, after user.cfg / autoexec: re-applied by quality preset Shading whenever that setting is (re)applied |
| `r_TexAtlasSize` | 3 | OVERRIDDEN | AT_RISK | re-applied by quality preset Texture at profile load, after user.cfg / autoexec: re-applied by quality preset Texture whenever that setting is (re)applied |
| `r_TexMaxAnisotropy` | 4 | OVERRIDDEN | AT_RISK | re-applied by quality preset Texture at profile load, after user.cfg / autoexec: re-applied by quality preset Texture whenever that setting is (re)applied; equals the registration default |
| `r_TexMinAnisotropy` | 2 | OVERRIDDEN |  | re-applied by quality preset Texture at profile load, after user.cfg |
| `r_TexturesStreamPoolSize` | 8 | OVERRIDDEN | AT_RISK | re-applied by quality preset TextureResolution at profile load, after user.cfg / autoexec: re-applied by quality preset TextureResolution whenever that setting is (re)applied; value '' is not a number (parses as 0) |
| `r_vsync` | 5 | OVERRIDDEN | AT_RISK | re-applied by menu: vsync at profile load, after user.cfg / autoexec: re-applied by menu: vsync whenever that setting is (re)applied |
| `sys_MaxFPS` | 5 | OVERRIDDEN | AT_RISK | re-applied by menu: fps cap (menu: 0/30/60) at profile load, after user.cfg / autoexec: re-applied by menu: fps cap (menu: 0/30/60) whenever that setting is (re)applied |
| `sys_spec_gameeffects` | 3 | OVERRIDDEN | AT_RISK | CVarGroup: applies every cvar in Config/CVarGroups/sys_spec_gameeffects.cfg; the menu's quality level replaces it at profile load / autoexec: CVarGroup: applies every cvar in Config/CVarGroups/sys_spec_gameeffects.cfg; the menu's quality level is re-applied whenever it's (re)applied |
| `sys_spec_objectdetail` | 3 | OVERRIDDEN | AT_RISK | CVarGroup: applies every cvar in Config/CVarGroups/sys_spec_objectdetail.cfg; the menu's quality level replaces it at profile load / autoexec: CVarGroup: applies every cvar in Config/CVarGroups/sys_spec_objectdetail.cfg; the menu's quality level is re-applied whenever it's (re)applied |
| `sys_spec_particles` | 3 | OVERRIDDEN | AT_RISK | CVarGroup: applies every cvar in Config/CVarGroups/sys_spec_particles.cfg; the menu's quality level replaces it at profile load / autoexec: CVarGroup: applies every cvar in Config/CVarGroups/sys_spec_particles.cfg; the menu's quality level is re-applied whenever it's (re)applied |
| `sys_spec_physics` | 3 | OVERRIDDEN | AT_RISK | CVarGroup: applies every cvar in Config/CVarGroups/sys_spec_physics.cfg; the menu's quality level replaces it at profile load / autoexec: CVarGroup: applies every cvar in Config/CVarGroups/sys_spec_physics.cfg; the menu's quality level is re-applied whenever it's (re)applied |
| `sys_spec_postprocessing` | 3 | OVERRIDDEN | AT_RISK | CVarGroup: applies every cvar in Config/CVarGroups/sys_spec_postprocessing.cfg; the menu's quality level replaces it at profile load / autoexec: CVarGroup: applies every cvar in Config/CVarGroups/sys_spec_postprocessing.cfg; the menu's quality level is re-applied whenever it's (re)applied |
| `sys_spec_shading` | 3 | OVERRIDDEN | AT_RISK | CVarGroup: applies every cvar in Config/CVarGroups/sys_spec_shading.cfg; the menu's quality level replaces it at profile load / autoexec: CVarGroup: applies every cvar in Config/CVarGroups/sys_spec_shading.cfg; the menu's quality level is re-applied whenever it's (re)applied |
| `sys_spec_shadows` | 3 | OVERRIDDEN | AT_RISK | CVarGroup: applies every cvar in Config/CVarGroups/sys_spec_shadows.cfg; the menu's quality level replaces it at profile load / autoexec: CVarGroup: applies every cvar in Config/CVarGroups/sys_spec_shadows.cfg; the menu's quality level is re-applied whenever it's (re)applied |
| `sys_spec_water` | 3 | OVERRIDDEN | AT_RISK | CVarGroup: applies every cvar in Config/CVarGroups/sys_spec_water.cfg; the menu's quality level replaces it at profile load / autoexec: CVarGroup: applies every cvar in Config/CVarGroups/sys_spec_water.cfg; the menu's quality level is re-applied whenever it's (re)applied |
| `e_shadows` | 1 |  | AT_RISK | re-applied by quality preset Shadows whenever that setting is (re)applied; equals the registration default |
| `q_ShaderWater` | 2 |  | AT_RISK | re-applied by quality preset Water whenever that setting is (re)applied |
| `r_ColorGrading` | 1 |  | AT_RISK | re-applied by quality preset PostProcessing whenever that setting is (re)applied; equals the registration default |
| `r_ShadowBlur` | 1 |  | AT_RISK | re-applied by quality preset Shadows whenever that setting is (re)applied |
| `sys_spec_texture` | 2 |  | AT_RISK | CVarGroup: applies every cvar in Config/CVarGroups/sys_spec_texture.cfg; the menu's quality level is re-applied whenever it's (re)applied |
| `sys_spec_sound` | 3 | GROUP | GROUP | CVarGroup: applies every cvar in Config/CVarGroups/sys_spec_sound.cfg |
| `cl_headbob` | 1 | OK |  |  |
| `Con_Restricted` | 6 | OK | OK |  |
| `e_MergedMeshesActiveDist` | 4 | OK | OK |  |
| `e_MergedMeshesOutdoorOnly` | 4 | OK | OK |  |
| `e_ShadowsAdaptScale` | 1 | OK |  |  |
| `e_ShadowsClouds` | 3 | OK | OK |  |
| `e_StreamCgfPoolSize` | 3 | OK | OK |  |
| `e_svoTI_DiffuseAmplifier` | 3 | OK | OK |  |
| `e_svoTI_MinReflectance` | 3 | OK | OK |  |
| `e_svoTI_PropagationBooster` | 1 | OK |  |  |
| `e_svoTI_Reflect_Vox_Max_Overhead` | 1 | OK |  |  |
| `e_svoTI_ResScaleBase` | 3 | OK | OK |  |
| `e_svoTI_SpecularAmplifier` | 3 | OK | OK |  |
| `e_svoTI_SSAOAmount` | 3 | OK | OK |  |
| `e_svoTI_TemporalFilteringBase` | 3 | OK | OK |  |
| `e_UberlodUpdateBudget` | 2 | OK | OK |  |
| `e_VolumetricFog` | 4 | OK | OK | equals the registration default / autoexec:  |
| `i_mouse_accel` | 4 | OK | OK | equals the registration default |
| `i_mouse_smooth` | 4 | OK | OK | equals the registration default |
| `pl_movement.power_sprint_targetFov` | 2 |  | OK |  |
| `r_AntialiasingTAAPattern` | 3 | OK | OK | equals the registration default / autoexec:  |
| `r_AntialiasingTAASharpening` | 3 | OK | OK |  |
| `r_BatchType` | 5 | OK | OK |  |
| `r_DepthBits` | 1 | OK |  |  |
| `r_DepthOfFieldMode` | 1 | OK |  |  |
| `r_DisplayInfo` | 3 | OK | OK | equals the registration default / autoexec:  |
| `r_DrawNearFoV` | 2 |  | OK |  |
| `r_Driver` | 1 |  | OK |  |
| `r_HDRBloom` | 1 | OK |  | int cvar: 0.4 truncates to 0 |
| `r_HDRGrainAmount` | 3 | OK | OK | equals the registration default / autoexec:  |
| `r_ShadersAsyncActivation` | 1 |  | OK | equals the registration default |
| `r_ShadersAsyncCompiling` | 1 |  | OK |  |
| `r_ShadersRemoteCompiler` | 1 |  | OK | equals the registration default |
| `r_ShadowsAdaptionMin` | 4 | OK | OK |  |
| `r_ShadowsAdaptionRangeClamp` | 5 | OK | OK |  |
| `r_ShadowsAdaptionSize` | 4 | OK | OK |  |
| `r_supersampling` | 3 | OK | OK |  |
| `r_texturesstreamingDeferred` | 1 | OK |  | equals the registration default |
| `r_TexturesStreamingMaxRequestedMB` | 5 | OK | OK |  |
| `r_TexturesStreamingMipBias` | 7 | OK | OK |  |
| `r_TexturesStreamingResidencyThrottle` | 6 | OK | OK |  |
| `r_TexturesStreamingResidencyTime` | 6 | OK | OK |  |
| `r_VolumetricFogMinimumLightBulbSize` | 1 | OK |  |  |
| `r_VolumetricFogShadow` | 1 | OK |  |  |
| `r_VolumetricFogTexDepth` | 1 | OK |  |  |
| `r_VolumetricFogTexScale` | 1 | OK |  |  |
| `sys_job_system_enable` | 3 | OK | OK | equals the registration default |
| `sys_limit_phys_thread_count` | 1 | OK |  |  |
| `sys_physics_CPU` | 4 | OK | OK |  |
| `wh_cs_playerhorizontalunlockdelay` | 1 | OK |  | flagged VF_DEPRECATED by the devs |
| `wh_cs_playerinputcombatunlockdelay` | 1 | OK |  | flagged VF_DEPRECATED by the devs |
| `wh_cs_playerinputcursorunlockdistance` | 1 | OK |  | flagged VF_DEPRECATED by the devs |
| `wh_cs_playerinputlockareawidth` | 1 | OK |  | flagged VF_DEPRECATED by the devs |
| `wh_cs_playerinputlockingtolerance` | 1 | OK |  | flagged VF_DEPRECATED by the devs |
| `wh_cs_playerinputmouseunlockminoppangle` | 1 | OK |  | flagged VF_DEPRECATED by the devs |
| `wh_cs_playerinputmouseunlockmintime` | 1 | OK |  | flagged VF_DEPRECATED by the devs |
| `wh_cs_playerinputmouseunlockreturntime` | 1 | OK |  | flagged VF_DEPRECATED by the devs |
| `wh_cs_playerinputmouseunlockxthreshold` | 1 | OK |  | flagged VF_DEPRECATED by the devs |
| `wh_cs_playerlockopponentanglebias` | 1 | OK |  | flagged VF_DEPRECATED by the devs |
| `wh_cs_playermaxopponentangletolock` | 1 | OK |  | flagged VF_DEPRECATED by the devs |
| `wh_cs_playermaxopponentangletounlock` | 1 | OK |  | flagged VF_DEPRECATED by the devs |
| `wh_cs_playermaxopponentdistancetolock` | 1 | OK |  | flagged VF_DEPRECATED by the devs |
| `wh_cs_playermaxopponentdistancetounlock` | 1 | OK |  | flagged VF_DEPRECATED by the devs |
| `wh_cs_playerunlocktimeout` | 1 | OK |  | flagged VF_DEPRECATED by the devs |
| `wh_cs_playerverticalunlockdelay` | 1 | OK |  | flagged VF_DEPRECATED by the devs |
| `wh_env_PuddlesRippleAmountMul` | 1 | OK |  |  |
| `wh_horse_CameraCentering` | 3 | OK | OK |  |
| `wh_pl_showfirecursor` | 6 | OK | OK |  |

## Quality-preset tables (Config/CVarGroups in engine.pak)
The profile applies these on top of user.cfg, at the level stored in attributes.xml (`sys_spec_*` = menu quality).
Cells already include inheritance: a level section that doesn't list a cvar uses the `[default]` section's value.

#### sys_spec_GameEffects (file default level = 3)

| cvar | 1 low | 2 med | 3 high | 4 v.high | 7 | settable from cfg |
|---|---|---|---|---|---|---|
| `mfx_Timeout` | 0.01 | 0.01 | 0.01 | 0.01 | 0.01 | yes |
| `r_RainOccluderSizeTreshold` | 10 | 10 | 10 | 10 | 10 | yes |
| `wh_env_PuddleCreationDelay` | 1000 | 1000 | 1000 | 1000 | 1000 | yes |
| `wh_env_PuddleCreationSpeed` | 0.005 | 0.005 | 0.005 | 0.005 | 0.005 | yes |
| `wh_env_PuddleDryupDelay` | 0 | 0 | 0 | 0 | 0 | yes |
| `wh_env_PuddleDryupSpeed` | 0.005 | 0.005 | 0.005 | 0.005 | 0.005 | yes |
| `wh_env_puddleMaskMin` | 0 | 0 | 0 | 0 | 0 | yes |
| `wh_env_DirtCreationSpeed` | 0.05 | 0.05 | 0.05 | 0.05 | 0.05 | yes |
| `wh_env_DirtDryupSpeed` | 0.05 | 0.05 | 0.05 | 0.05 | 0.05 | yes |
| `wh_env_RainDiffuseDarkening` | 0.2 | 0.2 | 0.2 | 0.2 | 0.2 | yes |
| `wh_env_RainWindStrength` | 10 | 10 | 10 | 10 | 10 | yes |
| `wh_env_RainDropsAmountMul` | 15 | 15 | 15 | 15 | 15 | yes |
| `wh_env_RainDropsSpeedMul` | 7 | 7 | 7 | 7 | 7 | yes |
| `wh_env_RainDropsSpeedBase` | 1.5 | 1.5 | 1.5 | 1.5 | 1.5 | yes |
| `wh_env_RainLayers` | 1 | 2 | 3 | 3 | 3 | yes |

#### sys_spec_Light (file default level = 3)

| cvar | 1 low | 2 med | 3 high | 4 v.high | 7 | settable from cfg |
|---|---|---|---|---|---|---|
| `r_Flares` | 0 | 0 | 0 | 0 | 0 | yes |
| `r_DeferredShadingAreaLights` | 0 | 1 | 1 | 1 | 1 | yes |

#### sys_spec_ObjectDetail (file default level = 3)

| cvar | 1 low | 2 med | 3 high | 4 v.high | 7 | settable from cfg |
|---|---|---|---|---|---|---|
| `ca_AttachmentCullingRation` | 145 | 300 | 360 | 370 | 450 | yes |
| `es_DebrisLifetimeScale` | 0.6 | 0.8 | 1 | 1 | 1 | yes |
| `e_CoverageBufferReproj` | 6 | 6 | 6 | 6 | 6 | yes |
| `e_DecalsAllowGameDecals` | 1 | 1 | 1 | 1 | 1 | yes |
| `e_DecalsLifeTimeScale` | 1 | 2 | 2 | 2 | 2 | yes |
| `e_DecalsOverlapping` | 1 | 1 | 1 | 1 | 1 | yes |
| `e_Dissolve` | 2 | 2 | 2 | 2 | 2 | yes |
| `e_LodMin` | 0 | 0 | 0 | 0 | 0 | yes |
| `e_LodRatio` | 10 | 15 | 25 | 45 | 60 | yes |
| `e_LodFaceAreaTargetSize` | 0.003 | 0.002 | 0.001 | 0.001 | 0.001 | yes |
| `e_MaxViewDistSpecLerp` | 0.5 | 1 | 1 | 1 | 1 | yes |
| `e_ObjQuality` | 1 | 2 | 3 | 4 | 4 | yes |
| `e_OcclusionCullingViewDistRatio` | 1 | 1 | 1 | 1 | 1 | yes |
| `e_StatObjBufferRenderTasks` | 1 | 1 | 1 | 1 | 1 | yes |
| `e_StreamCgf` | 1 | 1 | 1 | 1 | 1 | yes |
| `e_StreamInstancesMinLoadedNodes` | 2048 | 2048 | 2048 | 2048 | 2048 | NO (cheat/const) |
| `e_TerrainLodRatio` | 1 | 0.5 | 0.3 | 0.25 | 0.2 | yes |
| `e_TerrainOcclusionCullingMaxDist` | 130 | 200 | 200 | 200 | 200 | yes |
| `e_Tessellation` | 0 | 0 | 0 | 1 | 1 | NO (cheat/const) |
| `e_ViewDistMin` | 5 | 5 | 5 | 5 | 5 | yes |
| `e_ViewDistRatio` | 50 | 75 | 100 | 110 | 125 | yes |
| `e_ViewDistRatioCustom` | 60 | 100 | 100 | 130 | 150 | yes |
| `e_ViewDistRatioDetail` | 25 | 35 | 100 | 110 | 125 | yes |
| `e_ViewDistRatioLights` | 25 | 50 | 50 | 70 | 80 | yes |
| `e_ViewDistRatioVegetation` | 55 | 55 | 55 | 60 | 65 | yes |
| `r_FlaresTessellationRatio` | 0.25 | 1 | 1 | 1 | 1 | yes |
| `r_SilhouettePOM` | 0 | 0 | 0 | 0 | 0 | yes |
| `r_usezpass` | 2 | 2 | 2 | 2 | 2 | yes |
| `sys_flash_curve_tess_error` | 4 | 2 | 2 | 2 | 2 | yes |
| `e_UberlodDistanceRatio` | 1.5 | 1.6 | 1.8 | 2.2 | 2.5 | yes |
| `wh_cc_CharacterDetailReduction` | 1 | 0 | 0 | 0 | 0 | yes |
| `wh_cc_LodForItemStreamOutBase` | 11 | 15 | 15 | 20 | 20 | yes |
| `wh_cc_LodForAttachmentStreamOut` | 2 | 4 | 4 | 6 | 6 | yes |
| `WH_AI_LOD_DistanceMin` | 60 | 60 | 60 | 80 | 100 | not registered |
| `WH_AI_LOD_DistanceMax` | 90 | 90 | 90 | 110 | 130 | not registered |

#### sys_spec_Particles (file default level = 3)

| cvar | 1 low | 2 med | 3 high | 4 v.high | 7 | settable from cfg |
|---|---|---|---|---|---|---|
| `e_ParticlesMaxScreenFill` | 32 | 64 | 128 | 160 | 160 | yes |
| `e_ParticlesMinDrawPixels` | 1.5 | 1 | 1 | 1 | 1 | yes |
| `e_ParticlesMotionBlur` | 0 | 0 | 0 | 1 | 1 | yes |
| `e_ParticlesObjectCollisions` | 2 | 2 | 2 | 2 | 2 | yes |
| `e_ParticlesQuality` | 2 | 3 | 4 | 4 | 4 | yes |
| `r_ParticlesHalfRes` | 1 | 0 | 0 | 0 | 0 | yes |
| `r_ParticlesTessellation` | 0 | 1 | 1 | 1 | 1 | yes |
| `e_ParticlesLayeredUpdateMul` | 2 | 3 | 4 | 5 | 5 | yes |

#### sys_spec_Physics (file default level = 3)

| cvar | 1 low | 2 med | 3 high | 4 v.high | 7 | settable from cfg |
|---|---|---|---|---|---|---|
| `es_MaxPhysDist` | 50 | 100 | 100 | 100 | 100 | yes |
| `es_MaxPhysDistInvisible` | 15 | 25 | 25 | 25 | 25 | yes |
| `e_CullVegActivation` | 30 | 50 | 50 | 50 | 50 | yes |
| `e_FoliageWindActivationDist` | 10 | 25 | 25 | 25 | 25 | yes |
| `e_PhysOceanCell` | 1 | 0.5 | 0.5 | 0.5 | 0.5 | yes |
| `e_PhysProxyTriLimit` | 10000 | 10000 | 10000 | 10000 | 10000 | yes |
| `g_breakage_particles_limit` | 80 | 160 | 160 | 160 | 160 | yes |
| `g_tree_cut_reuse_dist` | 0.35 | 0 | 0 | 0 | 0 | yes |
| `p_gravity_z` | -9.81 | -9.81 | -9.81 | -9.81 | -9.81 | yes |
| `p_max_MC_iters` | 4000 | 6000 | 6000 | 6000 | 6000 | yes |
| `p_max_object_splashes` | 3 | 3 | 3 | 3 | 3 | yes |
| `p_max_substeps` | 2 | 5 | 5 | 5 | 5 | yes |
| `p_max_substeps_large_group` | 5 | 5 | 5 | 5 | 5 | yes |
| `p_num_bodies_large_group` | 100 | 100 | 100 | 100 | 100 | yes |
| `p_splash_dist0` | 7 | 7 | 7 | 7 | 7 | yes |
| `p_splash_dist1` | 30 | 30 | 30 | 30 | 30 | yes |
| `p_splash_force0` | 10 | 10 | 10 | 10 | 10 | yes |
| `p_splash_force1` | 100 | 100 | 100 | 100 | 100 | yes |
| `p_splash_vel0` | 4.5 | 4.5 | 4.5 | 4.5 | 4.5 | yes |
| `p_splash_vel1` | 10 | 10 | 10 | 10 | 10 | yes |
| `v_vehicle_quality` | 4 | 4 | 4 | 4 | 4 | yes |
| `e_MergedMeshesUseSpines` | 0 | 1 | 1 | 1 | 1 | yes |

#### sys_spec_PostProcessing (file default level = 3)

| cvar | 1 low | 2 med | 3 high | 4 v.high | 7 | settable from cfg |
|---|---|---|---|---|---|---|
| `q_ShaderHDR` | 1 | 1 | 2 | 3 | 3 | yes |
| `q_ShaderPostProcess` | 1 | 1 | 2 | 3 | 3 | yes |
| `r_ChromaticAberration` | 0 | 0 | 0 | 0 | 0 | yes |
| `r_ColorGrading` | 2 | 2 | 2 | 2 | 2 | yes |
| `r_ColorGradingChartsCache` | 4 | 0 | 0 | 0 | 0 | yes |
| `r_DepthOfField` | 2 | 2 | 2 | 2 | 2 | NO (cheat/const) |
| `r_DepthOfFieldDilation` | 0 | 0 | 0 | 0 | 0 | yes |
| `r_HDRBrightLevel` | 1.0 | 1.0 | 1.0 | 1.0 | 1.0 | not registered |
| `r_HDRRendering` | 1 | 1 | 1 | 1 | 1 | yes |
| `r_MotionBlur` | 2 | 2 | 2 | 2 | 2 | yes |
| `r_MotionBlurMaxViewDist` | 16 | 32 | 32 | 32 | 32 | yes |
| `r_MotionBlurQuality` | 0 | 1 | 1 | 2 | 2 | yes |
| `r_MotionBlurShutterSpeed` | 125 | 125 | 125 | 125 | 125 | yes |
| `r_PostProcessHUD3DCache` | 2 | 2 | 0 | 0 | 0 | yes |
| `r_RainMaxViewDist_Deferred` | 40 | 100 | 150 | 150 | 150 | yes |
| `r_Sharpening` | 0 | 0 | 0 | 0 | 0 | yes |
| `r_SunShafts` | 2 | 2 | 2 | 2 | 2 | yes |
| `r_TranspDepthFixup` | 0 | 1 | 1 | 1 | 1 | NO (cheat/const) |
| `r_AntialiasingMode` | 0 | 2 | 2 | 2 | 2 | yes |

#### sys_spec_Quality (file default level = 3)

| cvar | 1 low | 2 med | 3 high | 4 v.high | 7 | settable from cfg |
|---|---|---|---|---|---|---|
| `q_ShaderGeneral` | 1 | 1 | 2 | 3 | 3 | yes |
| `q_ShaderMetal` | 1 | 1 | 2 | 3 | 3 | yes |
| `q_ShaderGlass` | 1 | 1 | 2 | 3 | 3 | yes |
| `q_ShaderVegetation` | 1 | 1 | 2 | 3 | 3 | yes |
| `q_ShaderIce` | 1 | 1 | 2 | 3 | 3 | yes |
| `q_ShaderTerrain` | 1 | 1 | 2 | 3 | 3 | yes |
| `q_ShaderShadow` | 1 | 1 | 2 | 3 | 3 | yes |
| `q_ShaderFX` | 1 | 1 | 2 | 3 | 3 | yes |
| `q_ShaderSky` | 1 | 1 | 2 | 3 | 3 | yes |
| `q_Renderer` | 1 | 1 | 2 | 3 | 3 | yes |

#### sys_spec_Shading (file default level = 3)

| cvar | 1 low | 2 med | 3 high | 4 v.high | 7 | settable from cfg |
|---|---|---|---|---|---|---|
| `e_DynamicLightsMaxEntityLights` | 7 | 11 | 16 | 16 | 16 | yes |
| `e_GI` | 0 | 0 | 0 | 0 | 0 | yes |
| `e_GIAmount` | 0.6 | 0.6 | 0.6 | 0.6 | 0.6 | yes |
| `e_GIMaxDistance` | 50 | 100 | 100 | 100 | 100 | NO (cheat/const) |
| `e_GINumCascades` | 1 | 1 | 1 | 1 | 1 | yes |
| `e_LightVolumes` | 1 | 1 | 1 | 1 | 1 | NO (cheat/const) |
| `e_SkyUpdateRate` | 0.5 | 1 | 1 | 1 | 1 | yes |
| `e_TerrainAO` | 0 | 0 | 0 | 0 | 0 | yes |
| `e_VegetationUseTerrainColor` | 1 | 1 | 1 | 1 | 1 | yes |
| `r_DeferredShadingTiled` | 2 | 2 | 2 | 2 | 2 | yes |
| `r_DeferredShadingTiledHairQuality` | 1 | 1 | 1 | 2 | 2 | yes |
| `r_deferredShadingFilterGBuffer` | 0 | 0 | 0 | 0 | 0 | yes |
| `r_DetailDistance` | 4 | 8 | 8 | 8 | 8 | yes |
| `r_EnvTexUpdateInterval` | 0.075 | 0.05 | 0.05 | 0.05 | 0.05 | yes |
| `r_Refraction` | 1 | 1 | 1 | 1 | 1 | yes |
| `r_ssdoAmountAmbient` | 1 | 1 | 1 | 1 | 1 | yes |
| `r_ssdoAmountDirect` | 1.5 | 1.5 | 1.5 | 1.5 | 1.5 | yes |
| `r_ssdoAmountReflection` | 4 | 4 | 4 | 4 | 4 | yes |
| `r_ssdoRadius` | 0.3 | 0.3 | 0.3 | 0.3 | 0.3 | yes |
| `r_ssdoRadiusMax` | 2 | 2 | 2 | 2 | 2 | yes |
| `r_ssdoRadiusMin` | 0.1 | 0.1 | 0.1 | 0.1 | 0.1 | yes |
| `e_svoTI_DistantSsaoAmount` | 1 | 1 | 1 | 1 | 1 | yes |
| `r_ssdo` | 1 | 1 | 1 | 1 | 1 | yes |
| `r_ssdoHalfRes` | 2 | 2 | 2 | 2 | 2 | yes |
| `r_SSReflections` | 0 | 0 | 0 | 1 | 1 | yes |
| `r_DeferredShading3PL` | 1 | 1 | 2 | 2 | 2 | yes |
| `r_SSReflHalfRes` | 1 | 1 | 1 | 1 | 0 | yes |
| `r_DeferredShadingSSS` | 0 | 1 | 1 | 1 | 1 | yes |
| `sys_flash_edgeaa` | 1 | 1 | 1 | 1 | 1 | yes |
| `sys_spec_Quality` | 1 | 2 | 3 | 4 | 4 | not registered |
| `e_svoti_LowSpecMode` | 6 | 4 | 4 | 2 | 1 | yes |
| `e_svoTI_ConeMaxLength` | 25 | 25 | 25 | 30 | 35 | yes |
| `e_svoTI_DiffuseConeWidth` | 24 | 24 | 12 | 9 | 6 | yes |
| `e_svoTI_RsmConeMaxLength` | 2 | 3 | 6 | 8 | 12 | yes |

#### sys_spec_Shadows (file default level = 3)

| cvar | 1 low | 2 med | 3 high | 4 v.high | 7 | settable from cfg |
|---|---|---|---|---|---|---|
| `e_GsmLodsNum` | 4 | 4 | 5 | 5 | 5 | yes |
| `e_GsmRange` | 3 | 3 | 3 | 3 | 3 | yes |
| `e_ParticlesShadows` | 0 | 1 | 1 | 1 | 1 | yes |
| `e_Shadows` | 1 | 1 | 1 | 1 | 1 | yes |
| `e_ShadowsBlendCascades` | 0 | 1 | 2 | 2 | 2 | yes |
| `e_ShadowsCastViewDistRatio` | 0.6 | 0.8 | 1 | 1.3 | 1.6 | yes |
| `e_ShadowsCastViewDistRatioLights` | 0.2 | 0.2 | 0.2 | 0.5 | 0.8 | yes |
| `e_ShadowsLodBiasFixed` | 0 | 0 | 0 | 0 | 0 | yes |
| `e_ShadowsMaxTexRes` | 512 | 1024 | 1024 | 1024 | 1024 | yes |
| `e_ShadowsOnAlphaBlend` | 0 | 0 | 0 | 0 | 0 | yes |
| `e_ShadowsPoolSize` | 4096 | 4096 | 4096 | 4096 | 4096 | yes |
| `e_ShadowsResScale` | 4 | 4 | 4 | 4 | 4 | yes |
| `e_ShadowsTessellateCascades` | 1 | 1 | 1 | 1 | 1 | NO (cheat/const) |
| `e_ShadowsTessellateDLights` | 0 | 0 | 0 | 0 | 0 | NO (cheat/const) |
| `e_ShadowsUpdateViewDistRatio` | 60 | 80 | 100 | 100 | 100 | yes |
| `r_DrawNearShadows` | 1 | 1 | 1 | 1 | 1 | yes |
| `r_FogShadows` | 0 | 0 | 0 | 0 | 0 | yes |
| `r_FogShadowsWater` | 0 | 0 | 0 | 1 | 1 | yes |
| `r_ShadowBlur` | 0 | 3 | 3 | 3 | 3 | yes |
| `r_ShadowJittering` | 1 | 1 | 2.5 | 2.5 | 2.5 | yes |
| `r_ShadowPoolMaxFrames` | 30 | 30 | 30 | 15 | 5 | NO (cheat/const) |
| `r_ShadowPoolMaxTimeslicedUpdatesPerFrame` | 20 | 30 | 50 | 110 | 120 | yes |
| `r_ShadowsPCFiltering` | 1 | 1 | 1 | 1 | 1 | yes |
| `r_ShadowsCache` | 0 | 0 | 0 | 0 | 0 | yes |
| `r_ShadowsCacheResolutions` | 3162,2107,1405,936,312 | 6324,4214,2810,1872,624 | 6324,4214,2810,1872,624 | 6324,4214,2810,1872,624 | 6324,4214,2810,1872,624 | yes |
| `r_ShadowsUseClipVolume` | 1 | 1 | 1 | 1 | 1 | NO (cheat/const) |
| `e_ObjShadowCastSpec` | 1 | 2 | 3 | 4 | 7 | yes |
| `r_ShadowsScreenSpace` | 0 | 1 | 2 | 2 | 2 | yes |
| `e_SunAngleClamp` | 30 | 30 | 20 | 20 | 20 | yes |
| `r_ShadowCastingLightsMaxCount` | 10 | 12 | 16 | 22 | 28 | yes |
| `e_ShadowsCastViewDistRatioMulInvis` | 0.2 | 0.3 | 0.4 | 0.5 | 0.6 | yes |

#### sys_spec_Sound (file default level = 3)

| cvar | 1 low | 2 med | 3 high | 4 v.high | 7 | settable from cfg |
|---|---|---|---|---|---|---|
| `s_ObstructionMaxValue` | 0 | 0 | 0 | 0 | 0 | not registered |

#### sys_spec_Texture (file default level = 3)

| cvar | 1 low | 2 med | 3 high | 4 v.high | 7 | settable from cfg |
|---|---|---|---|---|---|---|
| `r_DynTexAtlasCloudsMaxSize` | 24 | 24 | 32 | 32 | 32 | yes |
| `r_DynTexAtlasSpritesMaxSize` | 16 | 16 | 32 | 32 | 32 | not registered |
| `r_DynTexMaxSize` | 100 | 120 | 160 | 160 | 160 | yes |
| `r_EnvCMResolution` | 0 | 1 | 2 | 2 | 2 | yes |
| `r_EnvTexResolution` | 1 | 2 | 3 | 3 | 3 | yes |
| `r_ImposterRatio` | 2 | 1.5 | 1 | 1 | 1 | yes |
| `r_TexAtlasSize` | 512 | 2048 | 2048 | 2048 | 2048 | yes |
| `r_TexMaxAnisotropy` | 2 | 8 | 4 | 16 | 16 | yes |
| `r_TexMinAnisotropy` | 2 | 8 | 4 | 16 | 16 | yes |
| `r_TexNoAnisoAlphaTest` | 1 | 1 | 0 | 0 | 0 | NO (cheat/const) |
| `e_TerrainDetailMaterialsViewDistZ` | 60 | 80 | 80 | 100 | 100 | NO (cheat/const) |

#### sys_spec_TextureResolution (file default level = 0)

| cvar | 1 low | 2 med | 3 high | 4 v.high | 7 | settable from cfg |
|---|---|---|---|---|---|---|
| `r_TexturesstreamingMinUsableMips` | 8 | 8 | 8 | 8 | 8 | yes |
| `r_TexturesStreamingSkipMips` | 2 | 1 | 0 | 0 | 0 | yes |
| `r_TexturesStreamPoolSize` | 384 | 1024 | 1536 | 2048 | 4096 | yes |

#### sys_spec_Vegetation (file default level = 3)

| cvar | 1 low | 2 med | 3 high | 4 v.high | 7 | settable from cfg |
|---|---|---|---|---|---|---|
| `e_MergedMeshesPool` | 8192 | 8192 | 8192 | 16384 | 16384 | yes |
| `e_MergedMeshesViewDistRatio` | 50 | 60 | 70 | 75 | 85 | yes |
| `e_ProcVegetation` | 0 | 0 | 0 | 0 | 0 | yes |
| `e_VegetationMinSize` | 0 | 0 | 0 | 0 | 0 | yes |
| `e_VegetationUseTerrainColor` | 1 | 1 | 1 | 1 | 1 | yes |
| `e_VegetationUseTerrainColorDistance` | 80 | 80 | 80 | 80 | 80 | yes |
| `e_MergedMeshesInstanceDist` | 2 | 4 | 8 | 12 | 16 | yes |
| `e_MergedMeshesLodRatio` | 1 | 4 | 5 | 6 | 8 | yes |

#### sys_spec_VolumetricEffects (file default level = 3)

| cvar | 1 low | 2 med | 3 high | 4 v.high | 7 | settable from cfg |
|---|---|---|---|---|---|---|
| `e_Clouds` | 1 | 1 | 1 | 1 | 1 | yes |
| `r_Beams` | 0 | 0 | 1 | 1 | 1 | yes |

#### sys_spec_Water (file default level = 3)

| cvar | 1 low | 2 med | 3 high | 4 v.high | 7 | settable from cfg |
|---|---|---|---|---|---|---|
| `e_WaterOceanFFT` | 1 | 1 | 1 | 1 | 1 | yes |
| `e_WaterTessellationAmount` | 1 | 10 | 10 | 10 | 10 | yes |
| `e_WaterTessellationAmountX` | 10 | 10 | 10 | 50 | 85 | yes |
| `e_WaterTessellationAmountY` | 10 | 10 | 10 | 50 | 85 | yes |
| `e_WaterTessellationSwathWidth` | 10 | 10 | 10 | 10 | 10 | yes |
| `q_ShaderWater` | 1 | 2 | 2 | 2 | 2 | yes |
| `r_WaterReflectionsMinVisiblePixelsUpdate` | 0.05 | 0.05 | 0.05 | 0.05 | 0.05 | yes |
| `r_WaterTessellationHW` | 0 | 0 | 0 | 1 | 1 | yes |
| `r_WaterUpdateDistance` | 1 | 1 | 0.2 | 0.2 | 0.2 | yes |
| `r_WaterUpdateFactor` | 0.1 | 0.05 | 0.0 | 0.0 | 0.0 | yes |
| `r_WaterVolumeCaustics` | 0 | 0 | 0 | 1 | 1 | yes |
| `r_WaterVolumeCausticsDensity` | 64 | 128 | 256 | 256 | 256 | yes |
| `r_WaterVolumeCausticsMaxDist` | 20 | 25 | 35 | 35 | 35 | yes |
| `r_WaterVolumeCausticsRes` | 384 | 512 | 1024 | 1024 | 1024 | yes |
| `r_WaterVolumeCausticsSnapFactor` | 1 | 1 | 1 | 1 | 1 | yes |
