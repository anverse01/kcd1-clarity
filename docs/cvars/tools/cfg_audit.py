"""Audit user.cfg / autoexec.cfg files against the engine's real cvar registry and config rules.
usage: python3 cfg_audit.py [--md out.md] file.cfg [file.cfg ...]   (Windows: py cfg_audit.py ...)
Uses ../data/cvars-WHGame-1.9.8.jsonl (cvar_registry.py) and ../data/cvargroups-WHGame-1.9.8.json (which cvars each
Config/CVarGroups/*.cfg in Engine/engine.pak sets). Every rule here is backed by ../cfg-loading.md."""
import argparse
import collections
import json
import os
import re
from pathlib import Path

DATA = Path(__file__).resolve().parent.parent / "data"
REGISTRY = DATA / "cvars-WHGame-1.9.8.jsonl"
GROUPS = DATA / "cvargroups-WHGame-1.9.8.json"
BLOCK = 0x2 | 0x1000000 | 0x2000000 | 0x800 | 0x40000000  # cheat | cheat_always | cheat_nocheck | readonly | locked

# Profile (Saved Games/kingdomcome/profiles/*/attributes.xml) re-applies these at load, see note section 4
MENU = {n.lower(): why for n, why in [
    ("r_Width", "resolution"), ("r_Height", "resolution"), ("r_Fullscreen", "display mode"),
    ("r_FullscreenWindow", "display mode"), ("sys_MaxFPS", "fps cap (menu: 0/30/60)"), ("r_VSync", "vsync"),
    ("cl_fov", "FOV slider (written straight into the cvar's storage)"), ("r_AntialiasingMode", "AA"),
    ("r_MotionBlur", "motion blur"), ("e_ViewDistRatio", "ViewDistRatio slider"),
    ("e_ViewDistRatioDetail", "ViewDistRatio slider"), ("e_ViewDistRatioLights", "ViewDistRatio slider"),
    ("e_ViewDistRatioPortals", "ViewDistRatio slider"), ("e_ViewDistRatioVegetation", "ViewDistRatio slider"),
    ("e_LodRatio", "LodRatio slider"), ("e_MergedMeshesViewDistRatio", "MergedMeshesRatio slider")]}
MENU_GROUPS = {g.lower() for g in ("sys_spec_ObjectDetail sys_spec_GameEffects sys_spec_light sys_spec_Particles "
                                   "sys_spec_Physics sys_spec_PostProcessing sys_spec_Shading sys_spec_Shadows "
                                   "sys_spec_Texture sys_spec_TextureResolution sys_spec_Water "
                                   "sys_spec_VolumetricEffects sys_spec_Vegetation").split()}
# Verified in IDA: registered, but nothing does anything with the value
DEAD = {
    "r_waterupdatethread": "backing var has no reader besides its registration",
    "sys_preload": "ICVar* stored at CSystem+0xAD0, never read",
    "sys_flash_address_space": "backing var has no reader besides its registration",
    "sys_taskthread4_cpu": "InitThreads only reads sys_TaskThread0..3_CPU (loop is i < 4)",
    "sys_taskthread5_cpu": "InitThreads only reads sys_TaskThread0..3_CPU (loop is i < 4)",
    "sys_pakstreamcache": "only adds Levels/<level>/_cache/ to the stream cache; KCD ships no _cache folders",
    "sys_budget_sysmem": "threshold for the CBudgetingSystem debug overlay; allocates/limits nothing",
    "sys_budget_videomem": "threshold for the CBudgetingSystem debug overlay; allocates/limits nothing",
    "e_particlesthread": "only compared against 0; any non-zero value equals the default (1)",
    "r_multigpu": "only selects GPU count for AFR; no effect on a single GPU",
}
# Read once during CSystem::Init after user.cfg is parsed and before autoexec.cfg runs
INIT_ONLY = {
    "sys_job_system_max_worker": "worker count = value (0 = logical CPU count), clamped to logical CPUs; default 8",
    "sys_main_cpu": "main thread affinity, set at thread-task-manager init",
    "sys_taskthread0_cpu": "task thread affinity", "sys_taskthread1_cpu": "task thread affinity",
    "sys_taskthread2_cpu": "task thread affinity", "sys_taskthread3_cpu": "task thread affinity",
    "sys_streaming_cpu": "stream-engine thread affinity mask = 1 << value",
}

# What each verdict means for the player (details in ../community-cfg-audit.md)
LEGEND = {
    "OK": "works",
    "OK_INIT": "works (read once at startup)",
    "TOO_LATE": "no effect: read at startup, before autoexec.cfg runs; move it to user.cfg",
    "OVERRIDDEN": "no effect: the game's video options replace it after user.cfg; try autoexec.cfg",
    "AT_RISK": "probably works, but undone whenever you change that video option (unverified)",
    "BLOCKED": "refused: cheat/const cvar, only works when the game runs with -devmode",
    "DEAD": "no effect: the game never uses this value",
    "MISSING": "no effect: not a cvar in this game version",
    "IGNORED": "not a valid line for this file",
    "GROUP": "sets a whole preset group",
}


def load_registry():
    reg = {}
    for line in open(REGISTRY):
        r = json.loads(line)
        reg.setdefault(r["name"].lower(), r)
    return reg


def load_groups():
    groups = collections.defaultdict(set)
    names = set()
    for g, members in json.loads(GROUPS.read_text()).items():
        names.add(g.lower())
        for k in members:
            groups[k.lower()].add(g)
    return groups, names


def parse(path):
    """Engine rules: lines split on CR/LF, trimmed; skipped if empty, starting with ';' or starting with '--'.
    user.cfg (CSystem::LoadConfiguration): needs '=', key/value trimmed, "quoted" value supported, anything after the
    value stays in it. autoexec.cfg (console 'exec'): each line goes to ExecuteString, which splits on ';' and accepts
    'name value' or 'name=value'."""
    kind = "autoexec" if "autoexec" in os.path.basename(path).lower() else "user"
    text = open(path, "rb").read().decode("utf-8", "replace")
    for raw in re.split(r"[\r\n]+", text):
        s = raw.strip()
        if not s or s.startswith(";") or s.startswith("--"):
            continue
        if kind == "autoexec":
            for cmd in filter(None, (c.strip() for c in s.split(";"))):
                k, _, v = cmd.partition("=") if "=" in cmd else cmd.partition(" ")
                yield kind, k.strip(), v.strip(" \t\"'"), raw.strip()
        elif "=" not in s:
            yield kind, None, None, raw.strip()
        else:
            k, v = s.split("=", 1)
            v = v.strip()
            m = re.match(r'^"(.*)"', v)
            yield kind, k.strip(), m.group(1) if m else v, raw.strip()


def number(v):
    m = re.match(r"^\s*([-+]?\d*\.?\d+(?:[eE][-+]?\d+)?)", v or "")
    return m.group(1) if m else None


def judge(kind, k, v, reg, groups, group_names):
    kl = k.lower()
    if kl in group_names:
        what = f"CVarGroup: applies every cvar in Config/CVarGroups/{k}.cfg"
        if kl not in MENU_GROUPS:
            return "GROUP", what
        if kind == "autoexec":
            return "AT_RISK", what + "; the menu's quality level is re-applied whenever it's (re)applied"
        return "OVERRIDDEN", what + "; the menu's quality level replaces it at profile load"
    r = reg.get(kl)
    if not r:  # sys_Pak* are registered through a CSystem helper (sub_180EB0938) the registry scan doesn't follow
        return ("DEAD", DEAD[kl]) if kl in DEAD else ("MISSING", "not a cvar in this build; silently ignored")
    fl, notes = r["flags"] or 0, []
    if fl & BLOCK or r["allow_modify"] is False:
        why = [n for b, n in ((0x2, "cheat"), (0x2000000, "cheat"), (0x1000000, "cheat"), (0x800, "read-only"),
                              (0x40000000, "locked")) if fl & b]
        if r["allow_modify"] is False:
            why.append("const")
        return "BLOCKED", f"{'/'.join(sorted(set(why)))}: refused unless the game runs with -devmode"
    n = number(v)
    if r["type"] in ("int", "float") and n is None:
        notes.append(f"value {v!r} is not a number (parses as 0)")
    if r["type"] == "int" and n and re.search(r"[.eE]", n):
        notes.append(f"int cvar: {n} truncates to {int(float(n))}")
    if fl & 0x4:
        notes.append("flagged VF_DEPRECATED by the devs")
    if kl in DEAD:
        return "DEAD", DEAD[kl]
    if kl in INIT_ONLY:
        if kind == "autoexec":
            return "TOO_LATE", INIT_ONLY[kl] + "; read during init, before autoexec.cfg runs"
        return "OK_INIT", INIT_ONLY[kl]
    over = []
    if kl in MENU:
        over.append("menu: " + MENU[kl])
    ms = sorted(g for g in groups.get(kl, ()) if g.lower() in MENU_GROUPS)
    if ms:
        over.append("quality preset " + "/".join(g.replace("sys_spec_", "") for g in ms))
    if n is not None and isinstance(r["default"], (int, float)) and not isinstance(r["default"], bool) \
            and float(n) == float(r["default"]):
        notes.append("equals the registration default")
    if over:
        tail = "; " + "; ".join(notes) if notes else ""
        if kind == "autoexec":  # order of profile load vs. autoexec.cfg is not verified yet (note section 4)
            return "AT_RISK", "re-applied by " + ", ".join(over) + " whenever that setting is (re)applied" + tail
        return "OVERRIDDEN", "re-applied by " + ", ".join(over) + " at profile load, after user.cfg" + tail
    return "OK", "; ".join(notes)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cfgs", nargs="+")
    ap.add_argument("--md")
    a = ap.parse_args()
    reg = load_registry()
    groups, group_names = load_groups()
    rows = []
    for path in a.cfgs:
        for kind, k, v, raw in parse(path):
            if k is None:
                rows.append((path, kind, raw, "", "IGNORED", "no '=': not a config line (logged as invalid)"))
                continue
            verdict, why = judge(kind, k, v, reg, groups, group_names)
            rows.append((path, kind, k, v, verdict, why))
    for path in a.cfgs:
        print(f"\n{path}")
        for _, kind, k, v, verdict, why in (r for r in rows if r[0] == path):
            line = f"{k} = {v}" if v else k
            print(f"  {verdict:<10} {line[:48]:<48} {why}")
    c = collections.Counter(r[4] for r in rows)
    print("\n" + ", ".join(f"{n} {verdict}" for verdict, n in c.most_common()))
    for verdict in c:
        print(f"  {verdict:<10} {LEGEND[verdict]}")
    if a.md:
        with open(a.md, "w") as f:
            f.write("| file | cvar | value | verdict | why |\n|---|---|---|---|---|\n")
            for path, kind, k, v, verdict, why in rows:
                f.write(f"| {os.path.basename(os.path.dirname(path))}/{os.path.basename(path)} | `{k}` | "
                        f"`{v[:24]}` | {verdict} | {why} |\n".replace("``", ""))
    return rows


if __name__ == "__main__":
    main()
