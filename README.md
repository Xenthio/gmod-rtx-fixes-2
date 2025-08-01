<img src="https://github.com/user-attachments/assets/fad469d4-b7b2-428c-a093-5b497f02d820" alt="drawing" width="500"/>

## Features
- Light Updaters
    - Forces Source to render all map lights
- Material fixes
    - Fixes some broken UI/game materials and removes detail textures
    - Change all water textures to a single one to simplify replacements in Remix
- Model fixes
    - Fixes some props having unstable hashes in RTX Remix so they can be replaced in the Remix Toolkit
    - Allows most HL2 RTX mesh replacements to load correctly
- Remix API support (x64 only)
    - Lua bindings for addon creation
       - Materials, config vars
    - Map-specific Remix settings
      

## Installation
1. Download [RTXLauncher](https://github.com/Xenthio/RTXLauncher/releases/latest).
2. Put `RTXLauncher.exe` in an empty folder (not in the same place as your game), run it as an <ins>**Administrator**</ins>.
3. Select `Run Quick Install` on the main screen and follow the prompts when asked.
4. Once it's finished, press `Launch Game` at the bottom of the launcher.

## Multiplayer
Multiplayer works best when the server/host has this addon and the cvar `sv_allowcslua 1` set.

You can join servers without the addon but you ***will*** experience visual issues.

## Incompatible Addons
| Type | Name | Reason |
| -------- | ------- | ------- |
| Map | [Bro Said He Knew A Spot 💀](https://steamcommunity.com/sharedfiles/filedetails/?id=3252367349) | Breaks other shader-skybox maps |
| Map | [gm_northbury](https://steamcommunity.com/sharedfiles/filedetails/?id=3251774364) | Rasterized |
| Map | [gm_bigcity_improved](https://steamcommunity.com/workshop/filedetails/?id=815782148) | Rasterized |
| Addon | [MW/WZ Skydive/Parachute + Infil](https://steamcommunity.com/sharedfiles/filedetails/?id=2635378860) | Consumes a lot of vram, most likely precaching |
| Addon | [CS:GO Weapons](https://steamcommunity.com/sharedfiles/filedetails/?id=2193997180) | Game freezes up on `Starting lua...` when loading into a map. <br>(can be worked around by typing `mat_dxlevel 95` in the console ingame, causes other issues though) |


## Known issues
### Vanilla
- Shader skyboxes (gm_flatgrass, gm_construct, etc) cannot be interacted with and may have rendering issues
- Some render targets (screenshots, whatever addons that rely on them) do not appear or behave strangely (investigating)
- NPC Eyes do not render as the fixed function rendering fallback no longer exists (investigating)
- Some maps are rasterized and also have vertex explosions.
- Some map lights will cull
- Enabling `r_3dsky` causes flickering in some maps
- Skinned meshes have unstable hashes

### Addons
- High vram usage from addons like ARC9 or TFA as they precache textures on map load
- Tactical RP scopes become invisible when using ADS

## Recommended Resources
[HDRI Editor](https://github.com/sambow23/hdri_cube/blob/main/README.md)

## Credits
* [vlazed](https://github.com/vlazed/) for [toggle-cursor](https://github.com/vlazed/toggle-cursor)
* Yosuke Nathan on the RTX Remix Showcase server for making the initial `Garry's Mod Remixed` logo
* Everyone on the RTX Remix Showcase server
* NVIDIA for RTX Remix
* [Nak2](https://github.com/Nak2) for [NikNaks](https://github.com/Nak2/NikNaks)
* [BlueAmulet](https://github.com/BlueAmulet) for [SourceRTXTweaks](https://github.com/BlueAmulet/SourceRTXTweaks)
* [0xNULLderef](https://github.com/0xNULLderef) and [Wolƒe Strider Shoσter](https://github.com/wolfestridershooter) for additional x64 patches (culling and HDR map lighting)
