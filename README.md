# Amnesia: A Machine for Pigs VR

A true virtual reality version of **Amnesia: A Machine for Pigs** for **SteamVR**, made with a **Valve Index** and also tested with an **Oculus Quest 2**.

It is not a VorpX-style wrapper. It is a **source port**: Frictional Games released the game's full source code (engine and game) under the GPLv3 in 2020, and this project builds its own game exe from that code with VR built into the renderer and the player code. It runs on **your own copy of the game** and only reads its files; it never changes the game folder.

## What you get
- **Stereo 3D** with the headset's exact per-eye projection, at the headset's full refresh rate (tested at 120 Hz).
- **6DoF head tracking and room-scale**: lean, step and **crouch for real**. Walls stop your body.
- **Motion controls with hands**: the fingers follow the Index finger tracking. You grab, carry and throw objects, and open doors, pull levers and turn valves with your hands (no telekinesis: touch it to use it).
- **Lantern from your hip**: take it out and put it away at your hip, and aim the light with your hand.
- **Both hands**: hold an object in one hand and open a door with the other. Small decorations (glasses, books, bottles) can be picked up too.
- **Comfortable locomotion**: stick walking, snap or smooth turning, no head bob, toned-down screen shake, no forced camera spins.
- **Menus, notes and the inventory** on a panel in the world, used with a laser pointer. The SteamVR keyboard is used for text fields.
- Every VR option is in `vr_settings.cfg`, a commented text file.

Saves and settings are shared with the normal game (`Documents\Amnesia\Pig`). The normal game keeps working as before.

---

## Requirements

| What | Notes |
|---|---|
| **Windows 10 or 11, 64-bit** | The game is 32-bit, which runs fine on 64-bit Windows. |
| **A VR-capable graphics card** | NVIDIA or AMD, roughly GTX 1070 / RX 5700 or better. |
| **The latest NVIDIA or AMD graphics driver** | The game needs proper OpenGL support. Restart after installing. |
| **Steam + SteamVR** and a **SteamVR headset** | A free Steam account is enough. Install SteamVR from Steam (Library > Tools) and finish its room setup. Tested with the **Valve Index** and the **Oculus Quest 2**. |
| **Your own copy of Amnesia: A Machine for Pigs** | **Epic Games Store** and **Steam** (app 239200): tested. Other stores: not tested. Start the normal game once to check it works. |

Not needed: the newer Visual C++ 2015-2026 runtime (the package contains `msvcp140.dll` and `vcruntime140.dll`), and the converted animations (they are made from your game files during installation or on the first start).

**Controllers**: default button bindings exist for **Index controllers**, **Quest 2/3 (Oculus Touch)**, **HTC Vive wands** and **Steam Frame controllers**. The Index and Quest 2 ones are tested on real hardware; Vive wands and Steam Frame controllers are not yet. Other controllers (WMR and others) work after you bind the actions yourself in SteamVR > Settings > Controllers > Manage controller bindings, where you can also change any default.

---

## Installation

You need the **VR package**: the `AmnesiaAMFP_VR` folder (about 11 MB). It contains no game files.

1. Install everything under [Requirements](#requirements).
2. Unzip the package anywhere and double-click **`install.bat`** in it. It:
   - finds your Epic or Steam copy of the game (if it can't, it asks for the folder with `aamfp.exe`);
   - copies the VR files into a `VR` folder inside the game's folder (Windows may ask for administrator rights when the game is under *Program Files*);
   - copies the game's engine DLLs and converts 98 character animations from your game files (a few seconds);
   - puts an **Amnesia AMFP VR** shortcut on your desktop.

   Running it again (for example after an update) keeps your `vr_settings.cfg`. You can delete the unzipped folder afterwards.
3. Start **SteamVR** and check that the headset and controllers are green.
4. Start **Amnesia AMFP VR** from the desktop shortcut. Windows SmartScreen may warn because the exe isn't signed: *More info* > *Run anyway*.
5. Put the headset on. **Recenter** with a right stick click (or F9) whenever you sit down or stand up.

**Without the installer**: copy the package folder anywhere (inside the game's folder is easiest) and start `aamfp_vr.exe` in it. It asks once where the game is installed if it isn't the parent folder, and does the DLL and animation steps itself on the first start. Always start `aamfp_vr.exe`, not `aamfp_vr_game.exe`.

**Settings**: open `vr_settings.cfg` in the `VR` folder with any text editor. Every option is explained in it.

**Without VR**: `aamfp_vr.exe -flat`.

**Uninstall**: delete the `VR` folder (and the shortcut).


## Button mappings

**Buttons (Index controllers)**: left stick walk (click = run), right stick snap turn (click = recenter), right trigger use/click, right grip grab (let go mid-swing to throw), left grip at your left hip = lantern, left A journal, left B pause menu, right A crouch, right B jump.

**Buttons (Quest 2/3)**: as on the Index, with X journal and Y pause menu. Finger curl comes from the touch sensors.

**Buttons (Steam Frame)**: as on the Index, with left d-pad down = journal and View (or right Menu) = pause menu.

**Buttons (Vive wands)**: left trackpad walk (click = run), right trackpad left/right turn; click the right trackpad up = jump, down = crouch, centre = recenter. Left menu button = pause menu, right menu button = journal. Trigger, grip and lantern as on the Index; the hands close with the grip and trigger.

---

## Building from source

Only needed to build the exe and the package yourself. On top of the requirements:

1. **Git for Windows** and this repository.
2. **Visual Studio 2026 Community** with the workload **"Desktop development with C++"** (MSVC v145, x86).
3. Internet on the first build: it downloads the OpenVR SDK v2.15.6 and unpacks `src\HPL2\dependencies.zip`.
4. Build the game: `src\build.bat all`.
5. Build the animation converter once: `src\tools\build_assimp.bat` (Assimp 6.0.5, FBX importer only), then `src\tools\fbx2anm\build.bat`.
6. Make the package and install it. PowerShell blocks scripts by default, so run them like this:
   ```
   powershell -ExecutionPolicy Bypass -File src\tools\package\make_package.ps1
   powershell -ExecutionPolicy Bypass -File src\tools\package\deploy_to_epic.ps1
   ```
   `make_package.ps1` builds `dist\AmnesiaAMFP_VR`. `deploy_to_epic.ps1` runs that package's `install.ps1` for the given game folder (the same installer players use). For a Steam copy use `deploy_to_steam.ps1`, which finds the game in your Steam libraries. Both accept `-Game "<folder>"`.
7. For a download zip: `src\make_release.bat` does a full rebuild, makes the package and zips it to `dist\amfp-vr-v<yyyyMMdd-HHmm>.zip` (with an empty `vr_package.cfg`).

For development, `run\` holds the working copy of the exe and its files, and `run\play_vr.bat` starts it against the Epic game folder.

## Project layout

| Path | What |
|---|---|
| `src\` | The game and engine source (Frictional's GPLv3 release plus the VR work) and `build.bat` |
| `src\HPL2\core\sources\vr\` | VR runtime (OpenVR): eyes, poses, input, overlay panel, keyboard |
| `src\Game\src\game\` | Game code: player, hands, interaction, comfort settings (`LuxVR.h`) |
| `src\tools\` | `fbx2anm` (animation converter), `hands` (hand model converter), `package` (package and install scripts) |
| `run\` | Development run folder: exe, DLLs, `vr_settings.cfg`, SteamVR input files, test launchers |
| `assets_src\` | Source of the hand models |

## Credits and licence
- Game and engine code: © Frictional Games, released under the **GNU GPL v3** ([FrictionalGames/AmnesiaAMachineForPigs](https://github.com/FrictionalGames/AmnesiaAMachineForPigs)). This VR version is GPLv3 as well.
- Hand models: Godot XR Tools hands by Miodrag Sejic, **CC0** (`run\vr_assets\hands\License.md`).
- OpenVR SDK © Valve (BSD-3), Assimp (BSD-3).
- The game's data (maps, models, sounds, textures) is **not** part of this project. You need your own copy of the game.
- Full licence texts: [`LICENSE`](LICENSE) (GPLv3) and [`THIRD_PARTY_NOTICES.txt`](THIRD_PARTY_NOTICES.txt).

## Disclaimer
Unofficial fan-made VR mod. It is not affiliated with, endorsed by or supported by Frictional Games or The Chinese Room. "Amnesia" and "A Machine for Pigs" are names of their respective owners; they are used here only to say which game this mod is for. You need your own copy of the game.

Finally, **this mod has been made via Claude AI**. If you do not support/don't want to use AI generated content, then do not use this mod.
