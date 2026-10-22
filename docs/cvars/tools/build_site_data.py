"""Builds ../site/data.json for the cvar web page (../index.html) from the files in ../data and the rule tables in
cfg_audit.py, so the page and the command-line audit apply the same rules.
usage: python3 build_site_data.py"""
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import cfg_audit  # noqa: E402

# CE3.8 IConsole.h flag bits (../cfg-loading.md section 3); 0x40000000 is KCD's own
FLAGS = {0x2: "CHEAT", 0x4: "DEPRECATED", 0x100: "DUMPTODISK", 0x800: "READONLY", 0x1000: "REQUIRE_LEVEL_RELOAD",
         0x2000: "REQUIRE_APP_RESTART", 0x20000: "WASINCONFIG", 0x80000: "RESTRICTEDMODE", 0x100000: "INVISIBLE",
         0x800000: "CONST", 0x1000000: "CHEAT_ALWAYS_CHECK", 0x2000000: "CHEAT_NOCHECK", 0x20000000: "RENDERER",
         0x40000000: "KCD_LOCKED"}


def main():
    cvars = []
    for r in cfg_audit.load_registry().values():
        flags = r["flags"] or 0
        if r["allow_modify"] is False:
            flags |= 0x800000
        d = r["default"]
        if isinstance(d, float) and d != d:  # NaN: a float the linear scan couldn't resolve
            d = None
        cvars.append([r["name"], r["type"], d, flags, r["help"] or ""])
    cvars.sort(key=lambda c: c[0].lower())
    out = {
        "build": "WHGame.dll 1.9.8 (Steam)",
        "cvars": cvars,
        "groups": json.loads(cfg_audit.GROUPS.read_text()),
        "menu": cfg_audit.MENU,
        "menuGroups": sorted(cfg_audit.MENU_GROUPS),
        "dead": cfg_audit.DEAD,
        "initOnly": cfg_audit.INIT_ONLY,
        "block": cfg_audit.BLOCK,
        "legend": cfg_audit.LEGEND,
        "flags": {str(k): v for k, v in FLAGS.items()},
        "rec": json.loads((cfg_audit.DATA / "recommendations.json").read_text()),
    }
    dst = HERE.parent / "site" / "data.json"
    dst.parent.mkdir(exist_ok=True)
    dst.write_text(json.dumps(out, separators=(",", ":"), ensure_ascii=False, allow_nan=False))
    print(f"{dst}: {len(cvars)} cvars, {dst.stat().st_size // 1024} KB")


if __name__ == "__main__":
    main()
