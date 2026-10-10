# Phase 1 report: vanilla source build

*2026-10-07. Result: **passed.** Our own `aamfp.exe`, built from Frictional's GPLv3 source with Visual Studio 2026, plays the Epic game data. The tester played it: smooth, UI, grabbing, doors, audio, save and load all work.*

## What we have now

| Item | Where |
|---|---|
| Source (git, local only) | `src\` — upstream commit tagged `upstream-2020`, our changes on top |
| Build script | `src\build.bat` (builds AngelScript, the engine and the game) |
| Built exe | `src\Game\redist\aamfp.exe` (32-bit, toolset v145) |
| Ready-to-run folder | `run\` — `aamfp.exe` + 12 runtime DLLs copied from the Epic install |
| Launcher | `run\play.bat` |
| Build logs | `phase1\logs\build_*.log` |
| Game logs | `phase1\logs\hpl.our_build.log` (our build), `hpl.epic_first_run.log` (Epic exe, for comparison) |
| Screenshots | `phase1\screens\` |

## Source changes

12 files, +69/−14 lines. All are build fixes; there are no gameplay changes.

| File | Change | Why |
|---|---|---|
| `HPL2/core/_HPL2_2010.vcxproj` | Toolset v145. Release defines: `HPL_NO_FBX`, `_CRT_SECURE_NO_WARNINGS`, `_CRT_NONSTDC_NO_DEPRECATE`, `_HAS_AUTO_PTR_ETC=1`, `WINDOWS_IGNORE_PACKING_MISMATCH`. `MeshLoaderFBX.cpp` excluded from Release. `/Wp64` and `/FD` dropped from the BuildID command. | Retarget from VS2010; FBX SDK 2012 is unavailable. |
| `HPL2/core/sources/impl/LowLevelResourcesSDL.cpp` | FBX include and `AddLoader(cMeshLoaderFBX…)` wrapped in `#ifndef HPL_NO_FBX` | No FBX SDK. |
| `HPL2/core/include/system/Platform.h` | `VideoComp` no longer derives from `std::binary_function` | Removed from the modern STL; the base only added typedefs. |
| `HPL2/core/include/graphics/Renderer.h`, `sources/graphics/Renderer.cpp` | `cRendererNodeSortFunc::operator()` made `const` | The modern STL requires const comparators for `std::multiset`. |
| `HPL2/core/include/gui/GuiSet.h`, `sources/gui/GuiSet.cpp` | `cGuiRenderObjectCompare::operator()` made `const` | Same reason. |
| `HPL2/core/include/input/InputTypes.h` | `cGamepadInputData::operator==` made `const` | `std::list::remove` needs a const `==`. |
| `Game/src/game/Lux.vcxproj` | Toolset v145; same Release defines; `fbxsdk-2012.2-md.lib` removed from the link | Retarget; no FBX SDK. |
| `Game/src/game/Lux.rc` | `afxres.h` → `winres.h` | `afxres.h` is MFC, which isn't installed (and isn't needed). |
| `build.bat` | New | One-command build. |
| `.gitignore` | Ignore unpacked dependencies, `HPL2/lib`, `Intermediate`, `Game/redist` | Keep build output out of git. |

`WINDOWS_IGNORE_PACKING_MISMATCH`: the bundled SDL1 headers set 4-byte packing around a `windows.h` include. VS2010 accepted this silently; the new Windows SDK asserts. The define keeps the original behaviour. Worth cleaning up later by fixing the include order.

Nothing else was needed:
- `LuxBasePersonal.h` is in the repo.
- The encrypted `gv_main_init_str` is only used with the `ptest` command line. Normally the game uses `config/main_init.cfg`.
- The BuildID pre-link step is disabled in the Release configuration.

## Libraries: reused vs rebuilt

| Library | Status |
|---|---|
| **AngelScript 2.24.1** | **Rebuilt** from `HPL2\dependencies\sources\angelscript` with v145 into `HPL2\lib`. The prebuilt VS2010 `angelscript.lib` fails with `LNK2038 _MSC_VER 1600 vs 1900`. |
| OALWrapper | Compiled as part of the engine project (source in `dependencies\sources\OpenAL`), as upstream does. |
| SDL2, GLEW, DevIL, zlibwapi, OpenAL32 + alut, Newton, Ogg/Vorbis/Theora, FLTK | **Reused** prebuilt `lib\win32` import or static libs; they link fine. |
| Runtime DLLs | Copied from the Epic install into `run\`: SDL2, zlibwapi, glew32, DevIL, libvorbisfile, libvorbis, libogg, libtheora, alut, OpenAL32, wrap_oal, Newton. |
| C runtime | Our exe uses the modern UCRT (`VCRUNTIME140`/`MSVCP140`, already on the PC) instead of the Epic exe's MSVCR100/MSVCP100. |

## How to build

From a clean checkout, with Visual Studio 2026 and the "Desktop development with C++" workload:

```
src\build.bat            (all: AngelScript, engine, game)
src\build.bat engine     (AngelScript + engine only)
src\build.bat game       (game only)
```

`build.bat`:
- puts the VS Installer folder on PATH, because VS 2026's `vcvarsall.bat` calls `vswhere.exe`;
- finds Visual Studio with `vswhere`;
- opens an x86 developer environment;
- unpacks `HPL2\dependencies.zip` if needed;
- runs MSBuild (Release, Win32) for AngelScript → `HPL2\_HPL2_2010.vcxproj` → `Game\src\game\Lux.vcxproj`.

The output is `src\Game\redist\aamfp.exe`. Copy it into `run\` to play it.

**Verified from scratch:** a fresh `git clone` of `src` built with a single `build.bat` call (unpacks the dependencies, builds AngelScript, the engine and the game) and produced `aamfp.exe` with exit code 0.

## How to run

Double-click `run\play.bat`. It changes the working directory to `C:\Program Files\Epic Games\AmnesiaAMFP` and starts `run\aamfp.exe`. The engine resolves all resources relative to the working directory: `config/main_init.cfg` → `resources.cfg` → data folders. The DLLs are found next to the exe. Settings, the log and saves go to `Documents\Amnesia\Pig` as with the Epic exe.

**The game folder stays untouched.**
- Before and after all test sessions it held 21,040 files, 5,372,314,561 bytes, and the newest file was still from the Epic install (17:50:48).
- `ForceCacheLoadingAndSkipSaving` is `true` (in `main_settings.cfg`, and also the code default), so the engine never writes `.msh`/`.anm` caches next to the data.

## Test results

| Test | Who | Result |
|---|---|---|
| Engine and game build with VS 2026 (v145) | Claude | ✓ |
| Intro and main menu | Claude | ✓ (`screens\01_main_menu.png`) |
| New game: `01_mansion_01` loads and renders, intro title plays | Claude | ✓ (`screens\03_level1_start.png`) |
| Smooth play, UI | The tester | ✓ |
| Grab objects | The tester | ✓ |
| Open doors | The tester | ✓ |
| Audio | The tester | ✓ |
| Save and load | The tester | ✓ (three autosaves in `Pig\test\`) |
| Windowed 1920×1080 | Claude | ✓; the original fullscreen 2560×1440 setting has been restored |

Automated input on the live desktop proved fragile: the Claude app can take focus, and the game uses its own relative-mouse cursor. The input helper `phase1\tools\gamectl.ps1` now refuses to send input or take screenshots unless the game window has focus. From now on, interactive testing is best done by the tester or with the game in a dedicated session.

## Known issues

1. **99 enemy/NPC `.FBX` animations don't load.** The log shows `ERROR: No loader for file extension 'fbx' found!` (9 times in this session). This affects the wretches, engineer, tesla, manpig, gent, maid and child animations. **Phase 1b fixes it** with a converter plus a cache overlay folder.
2. Content warnings also seen with the Epic exe:
   - `Sampler aGoboMap/aDiffuseMap does not exist`
   - `Could not find child with unique ID … to attach`
3. Other log errors, not yet compared against the Epic exe in-game (the Epic baseline log only covers the menu):
   - lamp-light links (`Light '…' that is connected to lamp '…' does not exist`, 144×)
   - a few missing sound/particle files (e.g. `C:/SVN/amnesia/redist/...static_rock_move_lr.snt`)
   - These look like original content bugs, but are worth a quick Epic-exe run of the same level to confirm.
4. The Epic exe contains FLTK and WinINet code that's absent from ours (per its RTTI and imports). They're probably an Epic-era crash or update dialog that isn't in the public source. They have no effect on gameplay.
5. The intro before the main menu takes about 30–40 seconds (also true of the original).
