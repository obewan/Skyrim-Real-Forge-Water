# Real Forge Water

SKSE plugin for Skyrim Special Edition that turns the static water of forge quench troughs into
real engine water: it reflects the sky, ripples when rain or anything else touches it, and moves
with the wind. ENB and Community Shaders treat it like any other water.

No forge mesh is replaced and no plugin file (ESP/ESL) is needed, so it works with any forge mod
without patches.

## How it works

Vanilla forge meshes have a built-in horse trough with a fake water plane: a flat shape drawn with
the normal object shader. When a forge loads, the plugin:

1. finds that water shape in whatever forge mesh is loaded (by its texture or name, not by a fixed
   mesh layout, so forge mesh replacers work too);
2. gives it the real water shader of the vanilla horse trough mesh, and moves it under a copy of
   that mesh's `BSMultiBoundNode` (the water system fills in that node's bounding box and crashes
   without it);
3. registers it with the game's water system (`TESWaterSystem::AddWater`), with a water type of its
   own: a copy, made in memory, of the vanilla horse trough water, with the settings below on top.

Real horse troughs elsewhere keep the vanilla water. When the forge unloads, its water is removed
from the water system.

### Realistic Water Two and other water mods

- **Water in forge troughs.** Realistic Water Two places its own real water at every vanilla forge,
  right on top of the forge water, with a film of dirt flecks under it. The plugin hides that water
  and its film in the troughs it converts (`bHideOtherTroughWater`), and the in-game menu can switch
  between the two to compare.
- **Wave textures.** Water mods replace the game's global wave texture, shared by all water. The
  forge water uses its own copy of the vanilla wave texture by default, so it keeps the vanilla
  waves while every other water keeps the water mod's. The menu or `sWaveTexture` can switch it to
  the global one, or to any other texture.

## Requirements

- [SKSE](https://skse.silverlock.org/)
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444)
- Optional: [SKSE Menu Framework](https://www.nexusmods.com/skyrimspecialedition/mods/120352) for the in-game menu

Built with CommonLibSSE-NG. Tested on Skyrim AE 1.7.104. The live update of the water look from
the menu uses a game address known for AE only: on SE it falls back to registering the water again,
which can lose its reflections until the next save load.

## Settings

`Data\SKSE\Plugins\RealForgeWater.ini`, read again each time a save is loaded:

- `[General]`: on/off (`iMode`), hiding other mods' trough water, verbose log.
- `[Water]`: the water type the forge water starts from, and its wave texture.
- `[WaterLook]`: reflections, colors and transparency, highlights, wind and waves (3 noise layers),
  ripples. An empty key keeps the value of the water type.

With SKSE Menu Framework, every `[WaterLook]` value is a live slider in
**Real Forge Water > Water look**, with a "Save to ini" button. Changes show when the menu is
closed, because the framework pauses the game while it is open. A new wave texture shows after
loading a save.

## Building

You need:

- Visual Studio 2022 or newer, or its Build Tools, with the C++ workload
- CMake 3.21 or newer
- [vcpkg](https://github.com/microsoft/vcpkg), with the `VCPKG_ROOT` environment variable set

Then run `build.cmd`, or from a Visual Studio developer prompt:

```
cmake --preset release
cmake --build build\release
```

The result is `build\release\RealForgeWater.dll`. The first configure downloads and builds the
dependencies and takes several minutes. CommonLibSSE-NG is built from the maintained fork by
alandtse through the vcpkg port in [`cmake/ports`](cmake/ports).

### Packaging

The mod archive also contains the vanilla wave texture, a Bethesda asset that is not stored here.
Extract it from your own game once (needs Python and `py -m pip install lz4`), then package:

```
py tools\extract_wave_texture.py "C:\...\Skyrim Special Edition\Data"
powershell -ExecutionPolicy Bypass -File tools\package.ps1
```

The result is `dist\RealForgeWater-<version>.zip`, laid out as the game's `Data` folder.

## Layout

| Path | Content |
|---|---|
| [`src/plugin.cpp`](src/plugin.cpp) | Plugin entry point, log, game events |
| [`include/Hooks.h`](include/Hooks.h) | `TESObjectREFR::Load3D` / `Set3D` hooks |
| [`include/ForgeScanner.h`](include/ForgeScanner.h) | Finds forges and the water shape in their mesh |
| [`include/WaterConverter.h`](include/WaterConverter.h) | Turns that shape into real water, the forge water type, other mods' trough water |
| [`include/WaterLook.h`](include/WaterLook.h) | Every water setting, shared by the INI and the menu |
| [`include/Settings.h`](include/Settings.h) | INI loading and saving |
| [`include/Menu.h`](include/Menu.h) | SKSE Menu Framework page |
| [`include/SKSEMenuFramework.h`](include/SKSEMenuFramework.h) | SKSE Menu Framework SDK header, with a fix to `ColorEdit3` |
| [`data`](data) | The INI shipped with the mod |
| [`tools`](tools) | Wave texture extraction and packaging |

## License

GPL-3.0, like CommonLibSSE-NG, which is compiled into the plugin. See [LICENSE](LICENSE).
