# Garry's Mod RTX Fixes 2
## Features
### Universal (x86 and x64)
- Light Updaters
    * Forces Source to render all map lights
- Water replacer
  * Replaces all map water materials with a single one so it can be replaced in Remix
- Material Fixer
    * Fixes some broken UI/game materials and removes detail textures
- Model fixer
    * Fixes props having unstable hashes in RTX Remix so they can be replaced in the Remix Toolkit
    * Allows HL2 RTX mesh replacements to load correctly
 
### x64 Only
- Custom World Renderer (required as x64's culling patches are not complete)
  * Renders map geometry with meshed chunks to prevent PVS/Frustrum culling of brush faces
- View Frustrum Forcing (required as x64's culling patches are not complete)
  * Modifies render bounds of static props and light updaters to prevent them getting culled by the view frustrum

## Installation
> [!WARNING]  
> It's recommended to use the 32-bit version of Garry's Mod instead of 64-bit due to unfinished engine patches and various other bugs.
> 64-bit should only be used for addons that only work there or to test new features from this repo.

1. Subscribe to [NikNaks](https://steamcommunity.com/sharedfiles/filedetails/?id=2861839844) on the Steam Workshop.
2. Download [RTXLauncher](https://github.com/Xenthio/RTXLauncher/releases/latest).
3. Put `RTXLauncher.exe` in an empty folder, run it as an Administrator.
4. Select `Run Quick Install` on the main screen and follow the prompts when asked.
5. Profit.

## Incompatible Addons
* (Map) [Bro Said He Knew A Spot 💀](https://steamcommunity.com/sharedfiles/filedetails/?id=3252367349) (Breaks other shader-skybox maps)

* (Map) [gm_northbury](https://steamcommunity.com/sharedfiles/filedetails/?id=3251774364) (rasterized)

* (Map) [gm_hinamizawa](https://steamcommunity.com/sharedfiles/filedetails/?id=3298456705) (vertex explosions and untextured draw calls)

* (Map) [gm_bigcity_improved](https://steamcommunity.com/workshop/filedetails/?id=815782148) (rasterized)

* (Addon) [MW/WZ Skydive/Parachute + Infil](https://steamcommunity.com/sharedfiles/filedetails/?id=2635378860)
   - Consumes a lot of vram, most likely precaching

## Known issues
### Vanilla
- Shader skyboxes (gm_flatgrass, gm_construct, etc) cannot be interacted with and may have rendering issues
- Some render targets (spawnmenu icons, screenshots, whatever addons that rely on them) do not appear or behave strangely (investigating)
- NPC Eyes do not render as the fixed function rendering fallback no longer exists (investigating)
- Some particles will not render (need to change each material from $SpriteCard to $UnlitGeneric to fix)
- HDR maps have no lighting (mat_fullbright 1 is forced if the SM isnt high enough)
- Some meshes will not render (limitation of FF rendering)
- Some maps will be rasterized and have vertex explosions.
- Some map lights will cull even with Lightupdaters active (investigating)
- Some func_ entities will cull in strange ways (investigating)
- Maps that don't extensively use PVS will have poor performance

### Addons
- High vram usage from addons like ARC9 or TFA as they precache textures on map load
- Tactical RP scopes become invisible when using ADS

## Recommended Resources and Addons
[HDRI Editor](https://github.com/sambow23/hdri_cube/blob/main/README.md)

## Credits
* [@vlazed](https://github.com/vlazed/) for [toggle-cursor](https://github.com/vlazed/toggle-cursor)
* Yosuke Nathan on the RTX Remix Showcase server for the gmod rtx logo
* Everyone on the RTX Remix Showcase server
* NVIDIA for RTX Remix
* [@BlueAmulet](https://github.com/BlueAmulet) for [SourceRTXTweaks](https://github.com/BlueAmulet/SourceRTXTweaks)  (We use this for game binary patching; Major thank you to BlueAmulet for their hard work)
