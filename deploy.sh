#!/usr/bin/env bash
# Copies the built mod into the game's Bin/Win64 (dinput8.dll is loaded from there by WHGame.dll).
#   ./deploy.sh          deploy build/dinput8.dll + dev nvngx_dlss.dll (DLSS debug overlay available)
#   ./deploy.sh --remove remove the mod files from the game folder
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
dst="${KCD_BIN:-/mnt/c/Program Files (x86)/Steam/steamapps/common/KingdomComeDeliverance/Bin/Win64}"
files=(dinput8.dll dinput8.pdb nvngx_dlss.dll)
if [[ "${1:-}" == "--remove" ]]; then
    for f in "${files[@]}"; do rm -fv "$dst/$f"; done
    exit 0
fi
cp -v "$here/build/dinput8.dll" "$here/build/dinput8.pdb" "$dst/"
cp -v "$here/build/ngx/dev/nvngx_dlss.dll" "$dst/"
echo "log: $dst/kcd_clarity.log"
