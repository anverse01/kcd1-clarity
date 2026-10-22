KCD1 Clarity - DLSS for Kingdom Come: Deliverance
==================================================

Unofficial mod. Not affiliated with Warhorse Studios or NVIDIA. Uses NVIDIA DLSS.

This is an early version. DLSS at 100% (DLAA) is the most reliable mode. Below 100%
some effects can still look wrong.
Please report what you see, with a screenshot and kcd_clarity.log.

Requirements
------------
- NVIDIA RTX graphics card (RTX 20 series or newer) with a recent driver.
- Kingdom Come: Deliverance (the first game), Steam version 1.9.8. Other versions
  (GOG, older patches) are untested; the mod tells you if it doesn't recognise the game.

Install
-------
1. Open the game folder (Steam: right-click the game > Manage > Browse local files).
2. Copy dinput8.dll and nvngx_dlss.dll into Bin\Win64 (next to KingdomCome.exe).
3. Start the game. In Options > Video, set Anti-aliasing to SMAA 2TX.
4. Press F8 in game, select DLSS and pick a quality level.

Your choices are saved in Bin\Win64\kcd_clarity.ini.

Conflicts
---------
Any other mod that also installs a dinput8.dll (Ultimate ASI Loader, other DLSS or
ASI mods) will overwrite this one or be overwritten by it. Only one can be installed.

Uninstall
---------
Delete dinput8.dll and nvngx_dlss.dll from Bin\Win64. You can also delete
kcd_clarity.ini and kcd_clarity.log.

Keys
----
F8  settings
F3  debug window (for bug reports)
The keys can be changed in kcd_clarity.ini ([ui] toggle_key / debug_key, Windows
virtual-key codes).

Licenses
--------
KCD1 Clarity is MIT licensed. nvngx_dlss.dll is NVIDIA's and covered by the NVIDIA
license in kcd_clarity_licenses. Source code, updates and bug reports: the GitHub
repository you downloaded this from (Issues tab).
