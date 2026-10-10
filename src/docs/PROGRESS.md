# MachineForPigsVR: progress log

*Read this first when resuming. Updated at every milestone. The copy on the tester's PC is `<project folder>\PROGRESS.md`.*

## Goal
True VR (SteamVR, Valve Index, motion controls) for Amnesia: A Machine for Pigs. Built as a **source port** of Frictional's GPLv3 code (github.com/FrictionalGames/AmnesiaAMachineForPigs). It runs the tester's Epic game data **read-only**. Full plan: `plan.md`.

**Defaults** (the tester hasn't overridden them): full motion controls, OpenVR first, personal use. The tester gives a go-ahead before each phase.

## Key paths (the test PC)
| What | Where |
|---|---|
| Project folder (all our files) | `<project folder>` |
| Epic game (READ-ONLY, never write) | `C:\Program Files\Epic Games\AmnesiaAMFP` |
| Saves/settings (shared with the flat game) | `%USERPROFILE%\Documents\Amnesia\Pig` (profile "test"; test runs use "dev_user") |
| Source (local git only, no pushes) | `src\` (tag `upstream-2020` = pristine upstream) |
| Run folder | `run\` (`aamfp.exe` + DLLs). Start with `run\play.bat` |
| Converted animations | `data_overlay\` (98 `.anm`, via `HPL_CACHE_OVERLAY` set in `play.bat`) |
| Reports | `phase0\`, `phase1\`, `phase1b\` (`*-report.md`) |
| Backups | `backups\Pig_2026-10-07.zip` (saves before any testing) |

## Build and run
- Build: `src\build.bat [all|engine|game]`. Needs VS 2026 Community with "Desktop development with C++" (installed). Output is Win32 Release, toolset v145.
- Run: `run\play.bat`, which sets cwd to the game folder and sets the overlay. Enemy test spot: `run\play_enemy_test.bat` (level 13, patrolling wretch; restores settings on exit; `run\restore_settings.bat` if interrupted).

## Done
- **Phase 0** (recon): the Epic exe is identical to source v1.30 (x86, VS2010, OpenAL, no Steam/EOS). Saves backed up. Report: `phase0\phase0-report.md`.
- **Phase 1** (vanilla build): built from source with VS2026; only AngelScript was rebuilt. The tester tested UI, grab, doors, audio and save/load: all OK. Report: `phase1\phase1-report.md`.
- **Phase 1b** (FBX): Assimp-based `src\tools\fbx2anm`, validated against 147 Frictional `.anm` files. New `cMeshLoaderFBXCache` serves `.fbx` from caches (without it, no enemy animations loaded at all). The tester confirmed the wretch animates. Report: `phase1b\phase1b-report.md`.
- **Phase 2** (seated VR): stereo with exact asymmetric per-eye frustums, head tracking, 120 Hz, world-locked menu panel (SteamVR overlay), `run\vr_settings.cfg`. The tester confirmed it in the Index on 2026-10-08 ("phase 2 done"). Reports: `phase2\phase2-report.md`, `phase2\phase2-polish-report.md`.

## Now
- **Phase 2 is complete** (2026-10-08). Waiting for the tester's go for **Phase 3** (locomotion and comfort).
- Phase 2 history (headset tests, bugs and fixes) is in `phase2\phase2-report.md` and `phase2\phase2-polish-report.md`.
- Tools that stay useful: `run\play_vr.bat`, `run\vr_settings.cfg`, `run\vr_selftest.ps1` (no-input VR test on a map, dev_user profile, restores settings), capture env vars `HPL_VR_CAPTURE_DIR` + `HPL_VR_CAPTURE_FRAMES` (eyes, GUI panel, compositor mirror if available), `HPL_VR_FIXED_POSE=1`, scripts in `src\tools\test`.

  **NEXT SESSION (when the tester gives the go for Phase 3):**
  1. Re-read `plan.md` Phase 3 and the deferred items below; propose a short plan to the tester.
  2. Phase 3 scope (from plan.md + deferred): head-relative movement and **interaction aim from the head** (today it aims with the invisible mouse pitch); body yaw from mouse/stick plus snap/smooth turn; room-scale/seated height offset, real crouch; VR policy for script camera control (look-ats, head spin, pitch fades: fade or body-yaw only); comfort pass on post effects (radial blur, image trail, insanity effects) with per-effect toggles in `vr_settings.cfg`; check the per-eye risks (occlusion queries, ImageTrail history, frame counter).
  3. Deferred to Phase 6: occasional stutters (the tester: leave for later).

## Next
Phase 3: locomotion and comfort. Phase 4: UI in VR. Phase 5: Index motion controls. Phase 6: polish and packaging. See `plan.md`.

## Phase 2 design decisions
- **Virtual screen:** in VR the engine's "screen" (framebuffer 0) is replaced by an eye-sized FBO per eye, so the whole pipeline (deferred renderer, post effects, GUI) renders unchanged. Screen size = eye render size; the desktop window is only a mirror (left eye, cropped).
- **Projection:** exact asymmetric (off-axis) per-eye frustum since 2026-10-08: `cFrustum` carries side tangents; all symmetric-FOV assumptions in the renderer were fixed (shaders didn't need changes). The old enclosing-symmetric + crop method stays as `Render Projection="Symmetric"` fallback (19% more pixels on the Index).
- **Camera:** gameplay camera untouched. Render eye = body position + body yaw (interpolated between 60 Hz logic steps, one step behind) x HMD pose x eye offset. Camera pitch/roll/FOV multipliers/shake rotation are ignored by the VR view. Mouse pitch still turns the (invisible) gameplay camera, which aims interaction; fix in Phase 3.
- **Frame pacing:** VR renders every compositor frame; engine FPS limiter and out-of-focus sleep are off in VR; logic still 60 Hz.
- **GUI:** 2D screen GUI rendered once per frame into a GUI panel texture shown as a SteamVR overlay fixed in seated space (Menu Distance/Width/Height in the cfg). Full-screen GUI objects (fades, flashes, full-screen images) are drawn in each eye instead. GUI draw lists must survive until the last eye pass.
- **Per-eye risks noted (not yet handled):** render frame counter increments per eye; occlusion queries and ImageTrail/motion-history effects may mix eyes. Check in headset; disable in VR if they misbehave.
- **Timing (Index 120 Hz, SteamVR recommended 3188x3540 = the tester's supersampling is high):** scale 1.0 (3869x3542/eye) ~9 ms, ~16% reprojected; scale 0.7 (2709x2480/eye) steady 8.33 ms.

## Gotchas learned
- Automated input on the tester's desktop is fragile. Only send it while the game window has focus, and never while the tester is playing.
- The engine resolves data relative to cwd, so run with cwd = game folder.
- The renderer assumed a symmetric FOV in several places; fixed 2026-10-08 (use `cFrustum` side tangents / `NearXToNormalized` for any new screen-space math).
- In HPL2, the camera view matrix and frustum feed game logic too (interaction ray, enemy sight, sanity). Apply the head pose to render cameras only.
- Shell escaping: writing paths like `sources\vr\...` through bash heredocs into Python turned `\v` into a vertical tab. Write patch scripts with the Write/Edit tools instead.
- `main_settings.cfg` has `LimitFPS="true" MaxFramesPerSec="60"`: without bypassing it VR runs at 60.
- The Claude app/other windows can steal focus: screenshots of the game need the game window in the foreground. For VR, use the capture env vars instead of screenshots.
- `02_mansion_02` starts with a scripted camera intro: two runs at the same frame number don't show the same view. Compare projections on the static main-menu scene.
- PowerShell `Remove-Item` on paths built from variables can be blocked by the harness; clear env vars with `[Environment]::SetEnvironmentVariable(name, $null, 'Process')`.
