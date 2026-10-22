# Perf / stutter cvar ranking (static + preset analysis, NOT benchmarked)

> **Not measured.** This ranking comes from reading the engine code, not from frame-time measurements. The
> autoexec.cfg placement (column A) also depends on an unverified load order ([`cfg-loading.md`](cfg-loading.md)
> section 4). Treat it as a list of things worth testing, and measure on your own machine.

Basis: what each cvar's code path does ([`cfg-loading.md`](cfg-loading.md)), how much Warhorse's own presets vary it
between Low and Very High (the tables in [`community-cfg-audit.md`](community-cfg-audit.md), i.e. their cost knobs),
and CryEngine 3.8 semantics. Impact is an estimate.

Where each line has to go (see [`cfg-loading.md`](cfg-loading.md) sections 3–4):
- **A** = autoexec.cfg: preset/menu members. user.cfg values are replaced at profile load; autoexec probably survives
  (unverified) but is undone when you change that menu option.
- **U** = user.cfg: init-time cvars. autoexec runs too late for them.
- **E** = either file: not touched by presets or menu.
- **D** = needs the `-devmode` launch option (cheat/const). Then it works from either file.

## Stutter / hitching
| # | cvar | where | rec. | why |
|---|---|---|---|---|
| 1 | `r_TexturesStreamPoolSize` | A | ~VRAM minus 2–2.5 GB (2 GB card: 1024, 4 GB: 1536–2048, 6 GB: 3072, 8 GB: 4096, 12 GB+: 6144) | the texture preset forces 1536 (High) / 2048 (V.High). Too small for the HD pack, so it thrashes (pop-in + hitches). Too big overcommits VRAM, which is worse |
| 2 | `r_TexturesStreamingMipBias` | E | 0 (2–3 GB cards: 1) | the popular -3 requests higher mips everywhere = more IO + VRAM |
| 3 | `sys_MaxFPS` (+ `r_VSync 0`, or a driver cap) | A | a cap you can hold, e.g. 45/50/72/90 | the menu only offers 0/30/60. An uncapped CPU-bound main thread starves the streaming/job threads, and a cap smooths frame pacing |
| 4 | `r_ShadowsCache` | A | keep 0 | Warhorse's own comment: cache spikes "every 10s" |
| 5 | `r_ShadowPoolMaxTimeslicedUpdatesPerFrame` | A | low 20, mid 30, keep preset above | caps shadow-pool updates per frame (preset 20/30/50/110) |
| 6 | `r_ShadersAsyncCompiling` | E | keep 3 (system.cfg) | 2 is a downgrade; 0 = stalls |
| 7 | `e_StreamCgfPoolSize` | E | 512 (system.cfg); 1024 with 32 GB RAM | mesh cache; below 512 = more re-streaming |
| 8 | `sys_streaming_max_finalize_per_frame` | E | experimental: 8–16 (0 = unlimited) | caps per-frame main-thread stream finalize work; trades pop-in speed for fewer spikes |
| 9 | `e_StreamCgfMaxTasksInProgress`, `e_StreamInstancesMaxTasks` | D | experimental: 64 on NVMe (default 32) | more parallel mesh/instance IO; less pop-in on fast drives |
| – | `e_AutoPrecacheCgf` | D | leave 1 (default) | 2 = "only important" meshes; community reports more pop-in |

## GPU frame time
| # | cvar | where | rec. (low / mid / mid-high) | why |
|---|---|---|---|---|
| 1 | `e_svoTI_LowSpecMode` | A | 6 / 4–5 / 2–4 | SVOTI GI internal resolution + shader cost; the preset goes 6→4→4→2→1, biggest single GPU knob |
| 2 | `e_svoTI_ResScaleBase` | E | keep 2 | 2 = half-res cone tracing; the configs' 0/1 ("pixelation fix") is a large cost increase |
| 3 | `e_ShadowsMaxTexRes`, `e_GsmLodsNum`, `e_ShadowsCastViewDistRatio` | A | 512,4,0.5 / 1024,4,0.8 / preset | sun-shadow cascades: resolution, count, caster distance |
| 4 | `r_ShadowCastingLightsMaxCount` | A | 6 / 10 / 12–16 | local shadowed lights (preset 10/12/16/22) |
| 5 | `r_DeferredShadingAreaLights` | A | 0 / 0–1 / 1 | complex area lights; only off on Low preset |
| 6 | `r_FogShadows`, `r_Beams`, `e_VolumetricFog` | A/E | don't enable (the configs set 1) | volumetric fog + its shadows are pure extra cost |
| 7 | `r_ssdo` / `r_ssdoHalfRes` | A | 0 or 2 / 2 / 2 | 0 = full res (cost) |
| 8 | `r_ParticlesHalfRes`, `e_ParticlesMaxScreenFill` | A | 1, 32 / 0, 64 / preset | fill-rate spikes in fires/smoke |
| 9 | `r_MotionBlur`, `r_SSReflections`, `r_WaterVolumeCaustics`, `e_ParticlesMotionBlur` | A | 0 | small; SSR/caustics/particle MB are V.High only |
| 10 | `r_DepthOfField` | D | 0 | const in retail; dialogue-only cost, low priority |

## CPU / draw calls (Rattay, forests)
| # | cvar | where | rec. (low / mid / mid-high) | why |
|---|---|---|---|---|
| 1 | `e_MergedMeshesViewDistRatio`, `e_MergedMeshesInstanceDist`, `e_MergedMeshesLodRatio` | A (+menu slider) | 50,2,1 / 60,4,4 / preset | grass/bush draw calls + animated (touch-bending) instances |
| 2 | `e_MergedMeshesUseSpines` | A | 0 / 1 / 1 | grass touch-bending simulation (off only on Low) |
| 3 | `e_LodRatio`, `e_ViewDistRatio*` | menu sliders | use the menu | applied from the profile anyway |
| 4 | `wh_cc_LodRatioRelative` | E | 0.5 / 0.6 / 0.7 (default) | NPC LOD distance relative to e_LodRatio. Crowds = KCD's main CPU cost |
| 5 | `wh_cc_LodForItemNoShadowsBase`, `wh_cc_LodForAttachmentNoShadows` | E | 2,0 / 3,1 / 4,1 (defaults) | from which character LOD items/attachments stop casting shadows |
| 6 | `ca_AttachmentCullingRation` | A | 145 / 300 / preset | attachment culling distance (preset 145/300/360/370) |
| 7 | `e_DynamicLightsMaxEntityLights` | A | 7 / 11 / 16 | lights per object |
| 8 | `es_MaxPhysDist`, `e_CullVegActivation`, `p_max_substeps` | A | 50,30,2 / preset | physics activation range |
| 9 | `sys_job_system_max_worker` | U | 0 | 0 = all logical CPUs; only changes anything above 8 threads (default 8) |

## Don't
`r_MultiThreaded` (already on, const) · any `sys_*_CPU` / `ca_thread*Affinity` pinning (fights the Windows scheduler,
bad on hybrid P/E-core Intel) · `r_ShadowsCache>0` · `e_svoTI_ResScaleBase 0` · `r_TexturesStreamingMipBias -3` ·
a texture pool bigger than VRAM · `e_StreamCgfPoolSize` < 512 · `r_ShadersAsyncCompiling 2` · cheat AI/char budgets
(`wh_ai_*Budget*`, `wh_cc_Scheduler*`, `wh_cc_Max*Jobs`) without measuring.
