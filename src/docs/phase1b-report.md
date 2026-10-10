# Phase 1b report: FBX animations without the FBX SDK

*2026-10-07. The game folder stayed read-only throughout: 21,040 files, 5,372,314,561 bytes, and the newest file is still from the Epic install.*

## Summary

- **Converter works and matches Frictional's output.** `fbx2anm` (Assimp + the engine's own `.anm` writer) reproduces the 147 Frictional-made `.anm` caches. 145 match structurally exactly; values agree to a fraction of a degree.
- **The 98 uncached FBX animations are converted** into `data_overlay\` (8.3 MB), which uses the same relative paths as the game folder.
- **Bigger finding: our Phase 1 build couldn't load *any* FBX-referenced mesh or animation, cached or not.** The original exe never reads the caches directly. A request for `child.FBX` goes to the FBX loader, which then opens `child.msh`. Our build had no FBX loader, so every child and wretch mesh/animation failed (`No loader for file extension 'fbx'`).
- **The fix:** a small engine class, `cMeshLoaderFBXCache`. It's compiled only with `HPL_NO_FBX` and serves `.fbx` requests from the caches: first from the overlay directory, then next to the source file. It never writes.
- **In-game check (automated, no input):**
  - Levels `01_mansion_01`, `02_mansion_02`, `11_engine_room` and `12_streets` now load with **0** FBX errors.
  - With our *own* converted animations as the overlay, `02_mansion_02` loaded **53** of them (child and wretch) without errors.
- **Visual confirmation of animations still needs the tester** (see the end of this report).

## 1. How it fits together

```
.ent  ──"child/animations/walk.FBX"──►  cFileSearcher (best path match, by file name)
                                        │
                                        ▼
                         cMeshLoaderHandler → loader for ".fbx"
                                        │
         Epic exe:   cMeshLoaderFBX ──► ForceCacheLoadingAndSkipSaving=true → walk.anm next to walk.FBX
         our build:  cMeshLoaderFBXCache ──► $HPL_CACHE_OVERLAY\entities\enemy\child\animations\walk.anm
                                         └─► else walk.anm next to walk.FBX
```

| File | What |
|---|---|
| `src/HPL2/core/include/impl/MeshLoaderFBXCache.h`, `sources/impl/MeshLoaderFBXCache.cpp` | New loader (≈150 lines). Logs `FBX cache overlay: …` at start-up and `Using overlay cache '…'` per hit. |
| `src/HPL2/core/sources/impl/LowLevelResourcesSDL.cpp` | Registers `cMeshLoaderFBXCache` when `HPL_NO_FBX` is defined. |
| `src/HPL2/core/_HPL2_2010.vcxproj` | Adds the two files. |
| `src/Game/src/game/LuxBase.cpp` | Trims trailing whitespace from the `-map` command-line argument, so the game can be started straight into a map for testing. |
| `run\play.bat` (also in `src\docs\`) | Sets `HPL_CACHE_OVERLAY=<project>\data_overlay` before starting the game. |

## 2. The converter: `src\tools\fbx2anm`

Build it with `src\tools\build_assimp.bat`, which fetches Assimp **v6.0.5** (BSD-3) into `src\tools\thirdparty` and builds it statically with only the FBX importer. Then run `src\tools\fbx2anm\build.bat`. No Autodesk SDK or licence is involved.

```
fbx2anm <in.fbx> <out.anm> [-v]
fbx2anm --list <file with "in.fbx<TAB>out.anm" lines>
```

It replicates `cMeshLoaderFBX::LoadAnimation` (`MeshLoaderFBX.cpp`):

| HPL2 + FBX SDK 2012 | fbx2anm |
|---|---|
| Skeleton = nodes with `GetSkeleton()` | FBX `Model` class `LimbNode`/`Root`/`Limb` (via Assimp's FBX DOM) |
| Rest pose = cluster `TransformLink × geometric transform` → T, R (XYZ euler), S → `T·R·S` | Same from `Cluster::TransformLink()`; rotation taken as a matrix (equivalent) |
| `MakeFinalBonesRec` (globals → locals; unlinked bones keep identity) | Same, in one pass |
| Take = first anim stack with a length > 0; length = stop in whole ms | Same; for these 2011-era files the span comes from the `Takes` section |
| Track per skeleton node, depth first, flags T\|R | Same order (Assimp keeps the FBX connection order) |
| Key times = union of the node's T/R/S curve key times, truncated to whole ms | Same |
| `EvaluateLocalTransform(t)` × geometric → T, q; `trans −= rest T`; `q = inv(rest R) · q` | Assimp node keys with pivots and pre/post rotations folded in (`PRESERVE_PIVOTS=false`), sampled at the truncated time (linear/slerp), then the same maths with the engine's `cMath`/`cQuaternion` |
| `cMeshLoaderMSH::SaveAnimation` | The same function: the tool links `HPL2_2010.lib` |

## 3. Validation against Frictional's 147 caches

All 147 FBX files that have a Frictional `.anm` were converted into `phase1b\scratch\gt\` and diffed with `anmtool.py` (results: `phase1b\gt_diff.json`).

| Check | Result |
|---|---|
| Files converted | 147 / 147, 0 errors |
| Take name, length | 147 / 147 identical |
| Track count, names, order, flags | 147 / 147 identical |
| Key count and key times per track | **145 / 147 identical**. The two maid cloth-sim files (`streets_sc7_cloth`, `streets_sc11_maid_run_alt_cloth`) get 1–3 extra keys at the end of some `SIMBONE` tracks. |
| Position, per-file max error | median **0.0002** units; worst 0.045 (child, a few keys) and 0.12 (the two cloth files) |
| Rotation, per-file max error | median **0.26°**; in the first test file p99 = 0.09°; worst 1.5° on very fast turns (not counting the two cloth files) |

The remaining differences come from one detail. FBX SDK evaluates the curves at whole milliseconds, e.g. 33 ms instead of frame 1 at 33.3 ms. That is 99 % of the way between two keys, using the curve's own interpolation (cubic, or step for a few child translation keys). We interpolate Assimp's keys linearly at the same time. Visually this is irrelevant: under 1.5° during fast motion, and exact at rest.

## 4. The 99 uncached files

- **98 animations** were converted into `data_overlay\…\*.anm`, with 0 failures.
- **1 mesh**, `entities/enemy/manpig/manpig_static.FBX`, was **not** converted. Only `manpig.ent` and `manpig_static_manequin.ent` use it, and no map or script references those. A mesh converter would be considerably more work (vertex splitting and so on), so it's left out on purpose.

Their tracks were checked against the skeletons the game uses:
- **engineer, tesla, wretch, child:** every track name exists in the character's `.msh` skeleton.
- **gent:** 13 `…Nub` tracks have no matching bone. Frictional's own animations carry these too, and the engine ignores them.
- **maid** and **dinkywretch:** the Epic data's `maid_actor.msh` and `dinkywretch.msh` are **25-byte empty meshes**, so these characters are invisible in the original game as well.

**Which of the 98 does the shipped game actually load?** Probably few, if any.
- The engine resolves animations by file name plus the best path match (`uncached_refs.py` mimics `cFileSearcher::GetFilePath`).
- With that logic, only `engineer_engroom.ent`, `engineer_nospawnsound.ent`, `engineer_tunnels.ent`, `child_scene11.ent`, `maid_actor.ent`, `dinkywretch.ent` and the manpig entities point at uncached files.
- None of these appear in a map, and `engineer_engroom` only appears in sound names in the scripts.
- Frictional's caches are generated when the game loads a file, so "no cache" most likely means "never loaded during development".

The overlay is still in place in case a script spawns one of them.

## 5. In-game tests (no input, no menu)

`phase1b\tools\run_map_test.ps1` starts the game straight into a map:
- It temporarily sets `ShowMenu`/`ShowPreMenu` to false and uses windowed 1920×1080; the original `main_settings.cfg` is restored afterwards.
- It runs `aamfp.exe -map <map>`, waits for "Loading complete", saves `hpl.log` and closes the window.

| Map | Overlay | FBX errors | Overlay hits |
|---|---|---|---|
| 01_mansion_01 | data_overlay | **0** (Phase 1 build: 9) | 0 |
| 02_mansion_02 | data_overlay | **0** | 0 |
| 11_engine_room | data_overlay | **0** | 0 |
| 12_streets | data_overlay | **0** | 0 |
| 02_mansion_02 | **our 147 converted GT anims** | **0** | **53** (child idle/walk/skip/run, wretch scenes, nest, …) |

Logs: `phase1b\logs\hpl.*.log`.

Cleanup after the tests:
- The no-menu start created a profile `dev_user` in `Documents\Amnesia\Pig`. It was moved to `phase1b\test_profile_dev_user` so the tester's Pig folder only has his `test` profile.
- `main_settings.cfg` is back to the original (fullscreen 2560×1440, menus on).

## 6. Still to do / needs the tester

1. **Visual check:** play `run\play.bat`, start a new game (or continue), and watch a child (Mansion 1–2) and a wretch (from Mansion 2 / the cellar on). They should move normally, not slide in a T-pose. In the Phase 1 build they would have been broken.
2. **Optional:** to see our converted animations instead of Frictional's, set `HPL_CACHE_OVERLAY` to `phase1b\scratch\gt` in `play.bat`.
3. **Known differences:** the two maid cloth files' extra keys and the sub-frame interpolation detail (section 3). Neither is used in place of Frictional's files, because their caches exist.
4. `manpig_static.FBX` mesh: not converted (unused).
