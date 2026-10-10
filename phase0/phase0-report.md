# Phase 0 report: recon and safety

*2026-10-07. The game folder and the Pig folder were treated as read-only; nothing was installed and the game was not launched.*

## Verdict

The Epic `aamfp.exe` is the same game as the released source. We can go ahead with the source port. Two things need dealing with before or during Phase 1:

1. **Visual Studio 2022 isn't installed.** It's needed to build anything.
2. **99 enemy/NPC animation `.FBX` files have no converted copy.** Our build has to load or convert them; we can't just drop FBX support.

## 1. Backup

- `backups\Pig_2026-10-07.zip`: 6 of 6 files, CRC test passed.
- No save games exist yet; the game has only been started once. The Pig folder holds `main_settings.cfg`, the logs, and profile `test\` (`user_settings.cfg`, `user_keys.cfg`).
- `main_settings.cfg`: `ForceCacheLoadingAndSkipSaving = true`. Screen is 2560×1440, fullscreen, vsync.
- `hpl.log`: "Version 1.30". NVIDIA GPU, OpenGL 4.6, driver 617.14.

## 2. The exe matches the source

| Check | Epic `aamfp.exe` | Released source | Match |
|---|---|---|---|
| Game version | "Version 1.30" in hpl.log | `kCurrentVersion_Main 1` / `Minor 30` (`LuxBase.cpp:123`) | ✓ |
| Architecture / compiler | x86, VS2010 runtime (MSVCR100/MSVCP100) | VS2010 projects, Win32 | ✓ |
| Build date | PE timestamp 2020-10-20 | Source released 2020-10-12 | ✓ |
| Build path | `D:\Amnesia\pig\Game\redist\aamfp.pdb` | `Game/redist` layout | ✓ |
| Libraries | SDL2, GLEW, DevIL, OpenAL + alut, Newton, Ogg/Vorbis/Theora, zlib | Same set in `dependencies.zip` | ✓ |
| Sound backend | OpenAL (`cLowLevelSoundOpenAL`) | OpenAL code present | ✓ (plan.md wrongly listed FMOD) |
| Classes | 777 RTTI names | 35 distinctive ones spot-checked (WaterLurker, PhoneBox, Infection areas, VoiceFlashback, EmotionStone, ClimbLedge…): all exist in source | ✓ |
| Online services | No Steam, EOS or Galaxy | n/a | ✓ (nothing to stub) |
| FBX | FBX SDK 2012.2 linked statically | `MeshLoaderFBX.cpp` uses FBX SDK 2012 | ✓ |

SHA256 `d9d8d1d9b093b346274a74fe9ddecaab1e08c25bd84abb9caabe62b989e9cb5b`, 8,437,760 bytes. A function-by-function Ghidra diff isn't needed given these matches. It stays available if something behaves differently later.

## 3. FBX: the one real snag

- `.dae`: 3,367 files, all cached except one stray unnamed file (harmless).
- `.FBX`: 255 files. 156 have a `.anm`/`.msh` cache; **99 don't**. All 99 are enemy and NPC animations (dinkywretch, engineer, tesla, wretch, manpig, gent, maid, child) plus `manpig_static.FBX`, and they're referenced by the `.ent` files.
- The Epic exe parses them at runtime with its built-in FBX SDK. A build without FBX support would break those animations.

**Options for Phase 1:**

| Option | How | Trade-off |
|---|---|---|
| A. One-time converter (recommended) | A small separate tool loads each FBX (modern FBX SDK or Assimp) and writes `.anm`/`.msh` using the engine's own MSH writer. The game build then needs no FBX support. | Conversion fidelity must be checked (compare animations in-game). The 99 cache files have to sit next to the FBX files, i.e. new files added inside the game folder (no originals touched), or in a copy of the data. |
| B. Port `MeshLoaderFBX` to Assimp | Replace the FBX SDK calls inside the engine. | More code in the engine; runtime parsing. |
| C. Find FBX SDK 2012.2 | Autodesk no longer offers it. | Probably a dead end. |

## 4. Dev tools on the PC

| Tool | Status |
|---|---|
| Visual Studio | Installer present, **no Visual Studio installed** |
| Git | 2.55.0 ✓ |
| Python | 3.14.5 ✓ |
| CMake | not found (comes with VS's C++ workload) |
| Java / Ghidra / IDA | not found (not needed for now) |

**Needed for Phase 1:** Visual Studio 2022 Community with the **"Desktop development with C++"** workload (includes MSVC v143, CMake and the Windows SDK; x86 build tools are part of it).

## Outputs on the PC (`<project folder>`)

- `backups\Pig_2026-10-07.zip`
- `phase0\pig_inventory.txt`, `game_inventory.txt`, `game_config\` (10 config files), `fbx_cache_check.txt`, `aamfp_imports.txt`, `aamfp_rtti.txt`, `aamfp_strings_paths.txt`, `devtools.txt`, and this report.
