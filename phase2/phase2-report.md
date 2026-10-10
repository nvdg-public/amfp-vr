# Phase 2 report: seated VR (stereo + head tracking)

*Completed 2026-10-08. The tester confirmed the result in the Index. Details of the second round: `phase2-polish-report.md`.*

## Result

**Exit criterion met.** the tester walks around levels in the Index with:
- correct-scale stereo ("scale perfect");
- neutral 1:1 head tracking;
- smooth motion at 120 Hz;
- a sharp image;
- menus, subtitles and hints on a readable floating panel.

He described it as "very comfortable".

## What Phase 2 delivered

| Area | Result |
|---|---|
| Launch | `run\play_vr.bat` (VR) next to the unchanged `run\play.bat` (flat). Falls back to flat if SteamVR or the headset is missing. |
| Settings | `run\vr_settings.cfg`, documented: render scale, projection, edge smooth, refresh hint, menu panel, recenter key, mirror window |
| Rendering | Engine "screen" = an eye-sized framebuffer per eye. Exact asymmetric per-eye frustums. 3254×3540 per eye at scale 1.0 on the tester's setup, steady 120 Hz in tests. |
| Head tracking | Seated universe, F9 recenter. Body position and yaw from the game, interpolated between the 60 Hz logic steps; head pose on top. |
| Menus/HUD | World-locked SteamVR overlay panel, 2 m ahead and 2 m wide. Fades and flashes still cover the whole view. Main-menu dialogs laid out for a 1080-high virtual screen. |
| Desktop | Mirror window (left eye); the mouse on it maps to the menus |
| Debug | Timing log every 500 frames; per-eye, panel and compositor captures; fixed-pose mode; image-comparison scripts |

## Bugs found by the tester and fixed

1. **Blurry:** fixed by exact asymmetric projection, scale 1.0 and edge smoothing off in VR.
2. **Profiles window flickering in the right eye:** the GUI draw list was emptied after the left eye.
3. **Menus "on the face", small text:** fixed by the world-locked overlay panel.
4. **Panel upside down:** a double v-flip.
5. **Intro logos seen double:** full-screen images in asymmetric eye images; they now go on the panel.
6. **Profile and Settings windows too small:** the dialogs were laid out in eye-buffer pixels.

## Deferred, by agreement or by plan

- **Occasional stutters:** the tester said to leave them for later. Phase 6 performance work: check the VR timing log for reprojection, hitches from the 60 Hz logic step, shader and texture loading. Running at 90 Hz is an easy test.
- **Interaction:** still aims with the invisible mouse pitch, not the head (Phase 3). The crosshair and hand icon are on the panel (to be removed in Phase 4). The arm and lantern model are visible (Phase 5).
- **Per-eye risks not observed so far:** render-frame counter per eye, occlusion queries, and the ImageTrail/RadialBlur post effects in VR. Check during the Phase 3 comfort pass.
- **Seated only:** room-scale, crouch, snap turn and script camera policies are Phase 3.

## Main code locations

- `src/HPL2/core/{include,sources}/vr/VRSystem.*`: all OpenVR code.
- Hooks in:
  - `LowLevelGraphicsSDL.cpp`, `FrameBufferGL.cpp`: virtual screen, mirror.
  - `Engine.cpp`: frame loop.
  - `Scene.cpp`: per-eye frustum, GUI panel pass.
  - `GuiSet.cpp`: panel/full-screen split.
  - `MouseSDL.cpp`.
  - `Frustum.*`, `Math.cpp`, `Renderer*.cpp`: asymmetric frustums.
  - `RendererDeferred.cpp`: edge smooth.
  - `Game/src/game/LuxMainMenu.cpp`: menu layout size.
- See `PROGRESS.md` for design decisions and gotchas.
