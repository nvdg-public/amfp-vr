# Phase 2 polish report

*2026-10-08. These are the four "NEXT SESSION" items from the tester's first headset test, done in one session. Commits `84b9b4e` … `48dddf3` (local git only). All checks below were automated without input and without the headset worn; the tester's headset test is still to come.*

## Summary

| # | Item | Result |
|---|---|---|
| 1 | VR config file | `run\vr_settings.cfg`: documented, read at start-up, overridable for tests |
| 2 | Blurriness | Exact asymmetric per-eye projection (16% fewer pixels for the same view), default scale raised 0.7 → 1.0, edge-smoothing blur off in VR |
| 3 | Right-eye flicker (profiles window) | Root cause found and fixed; per-eye captures now show the GUI identically in both eyes |
| 4 | Menus pressed against the face | Menus/HUD moved to a crisp, world-locked SteamVR overlay panel (2 m ahead, 2 m wide, configurable) |

## 1. `run\vr_settings.cfg`

HPL's XML cfg format, the same as the game's own settings. Every key is documented in the file.

| Section / key | Default | Meaning |
|---|---|---|
| `Render Scale` | 1.0 | Per-eye resolution relative to SteamVR's recommended size (which already includes the SteamVR supersampling setting) |
| `Render Projection` | Asymmetric | `Asymmetric` = exact headset FOV per eye. `Symmetric` = old enclosing+crop method (fallback). |
| `Render EdgeSmooth` | false | The game's edge-smoothing pass, a screen-space blur, in VR |
| `Display RefreshRateHint` | 0 | Only logs a note if SteamVR runs at a different rate; the rate itself is set in SteamVR |
| `Menu Distance / Width / Height` | 2.0 / 2.0 / 0.0 m | GUI panel placement relative to the recentered head |
| `Input RecenterKey` | F9 | F1–F12 or a letter/digit; works without window focus |
| `Mirror Enabled / Width / Height` | true / 1280 / 720 | Desktop mirror window |

- `HPL_VR_CONFIG` selects another file; `HPL_VR_SCALE` and `HPL_VR_MIRROR` still override for tests.
- `run\play_vr.bat` now only sets `HPL_VR=1` and the animation overlay.
- World scale is not a setting (the tester: it's perfect).

## 2. Blurriness

**What changed:**
- **Exact asymmetric eye frustums.** Every place in the engine that assumed a symmetric FOV now uses per-side tangents:
  - `cFrustum`: new asymmetric setup, side tangents, normalisation helpers.
  - `cMath` clip rects: light scissors, fog boxes, reflections.
  - `RendererDeferred`: far-plane corners and the fullscreen quads used to reconstruct positions; the reflection frustum and its near-plane clip.
  - `Renderer`: near-plane radius.
  - The frustum bounding sphere.
  - The shaders needed no change: lights reconstruct from view-space vertices, and the quads get their corners from the CPU.
- **Default render scale 0.7 → 1.0.** At 1.0 the Index renders 3254×3540 per eye (the old method needed 3869×3542 for the same view).
- **Edge smoothing off in VR.** It blurs edges in screen space, which only softens an already supersampled image.
- **Checked:**
  - Submit sends the full-resolution texture, with no crop or downsampling any more.
  - The mirror window only downsamples for the desktop.
  - MSAA is not an option with the deferred G-buffer.

**Validation (main-menu scene, fixed head pose, same frame in two runs):**
- I compared the old symmetric render, cropped to the eye's frustum, with the new asymmetric render: mean difference **0.4/255**, and the diff image only shows resampling edges (`phase2\captures\menu_compare_left.png`).
- No missing or misplaced lights, scissors, fog or reflections.
- The first in-level comparison differed only because `02_mansion_02` starts with a scripted camera intro.

**Timing (Index at 120 Hz, 8.33 ms budget; headset idle, so GPU load is lower than in real use):**

| Projection | Scale | Per-eye size | App interval | Render+submit (CPU) |
|---|---|---|---|---|
| Symmetric | 1.0 | 3869×3542 | 8.33 ms | 3.7 ms |
| **Asymmetric** | **1.0** | **3254×3540** | **8.33 ms** | **3.4 ms** |
| Asymmetric | 1.2 | 3905×4248 | 8.33 ms | 4.4 ms |

**Recommendation for the tester:** SteamVR's recommended size is about 2.2× the Index panel per axis, because his SteamVR "Render resolution" is high. If the headset test shows reprojection or stutter, setting the Index to 90 Hz in SteamVR > Settings > Video gives a 33% larger frame budget. I didn't change his SteamVR settings myself.

## 3. Right-eye flicker

**Cause:**
- The game fills each GUI set's draw list once per frame (`cGui::OnDraw`), but `cGuiSet::Render` emptied the list after rendering.
- In VR the scene renders once per eye, so the right eye drew from an empty or partly refilled list.

**Fix:** the list is kept until the last eye (`cVRSystem::SetRenderingEye` / `IsRenderingLastEye`).

**Checked:** the new per-eye capture option (`HPL_VR_CAPTURE_DIR` + `HPL_VR_CAPTURE_FRAMES`) dumps both eyes. The profiles window is now identical in both (`phase2\captures\pair_frame7000.png`). The game's own per-frame effect lists are cleared in the logic update, so they were already safe.

## 4. Menus on a world-locked panel

**How it works:**
- Each frame the 2D screen GUI is rendered once into a screen-sized texture with alpha.
- SteamVR shows that texture as an **overlay** fixed in the seated tracking space, by default **2.0 m ahead, 2.0 m wide** (about 53° wide, 2.18 m tall).
- The compositor draws it directly at display resolution in both eyes. So it's sharp, has no per-eye differences, sits at a real distance instead of "on the face", and recentering (F9) brings it back straight ahead.

**Split:**
- GUI objects covering the whole screen stay full-view in each eye: fades, flashes, full-screen images, the in-game menu background.
- Everything else goes to the panel: menus, notes, inventory, hints, subtitles, the crosshair/hand icon and the mouse cursor.

**Checked:**
- The panel texture contains the menus and is transparent elsewhere: 1.7% of pixels have alpha at the profiles dialog, and the full main menu renders on it.
- The eye images no longer contain menus.
- Flat mode is unaffected.

**Not verifiable without the headset:** the final placement and orientation of the overlay. The GL texture's vertical flip is set via texture bounds; if the panel appears upside down, that's a one-line fix. SteamVR's mirror texture isn't available while the headset is idle.

**Coordinator's default of 1.5 m wide:** I chose 2.0 m because the tester found the text small; it's in the config.

## Other changes and notes

- **Debug options:**
  - `HPL_VR_FIXED_POSE=1` (identity head pose, for reproducible captures).
  - Capture also writes the GUI panel and, when available, the compositor mirror.
  - Comparison tools: `src\tools\test\compare_projection.ps1`, `pair_captures.ps1`, `preview_bright.ps1`.
- **Test runs:**
  - Used the `dev_user` profile.
  - `main_settings.cfg` is identical to the pre-test backup.
  - The game folder is unchanged.
- **Still head-locked or unchanged (Phase 3/4):**
  - The crosshair is now on the panel, which is better but still not a proper VR reticle.
  - Interaction still aims with the invisible mouse pitch.
  - Arm and lantern model.
  - Post effects untuned.
- **Fades:** they cover the whole view, but the panel (an overlay) is always drawn on top. During a fade to black, HUD text on the panel stays visible, as in the flat game.

## Headset test: what the tester should check

See the message in the project thread; it's the same list as below.
1. Sharpness at the default (Scale 1.0, asymmetric, no edge smooth). Is it better than last time?
2. Smoothness: any judder or stutter? If yes, try 90 Hz in SteamVR.
3. Profiles window at boot: identical in both eyes now?
4. Menus: is the panel upright, about 2 m ahead, readable? Does it stay put when you turn your head, and come back with F9? Is the size right (`Menu Width` / `Distance` in `vr_settings.cfg`)?
5. In-game: hints/subtitles/crosshair on the panel, fades covering the full view, anything missing or doubled.
6. Any edge of the view looking wrong (black borders, lights cut off at the sides): that would indicate an asymmetric-frustum issue. As a check, `Render Projection="Symmetric"` brings back the old method.
