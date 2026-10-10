# MachineForPigsVR: plan of approach

*Phase 1 (analysis) output. Written 2026-10-07.*

Goal: play **Amnesia: A Machine for Pigs** (AMFP) in true VR on **SteamVR with a Valve Index**: stereo rendering with per-eye projection, 6DoF head tracking, room-scale, and Index controller interaction. Game data comes from the tester's Epic Games install.

---

## 1. The headline finding: we don't need to reverse-engineer the game

Frictional Games released the **full AMFP source code** (engine and game) under GPLv3 in September 2020:

- **github.com/FrictionalGames/AmnesiaAMachineForPigs**: `HPL2/core` (engine), `Game/src/game` (all ~78k lines of game code: player, interaction states, lantern, enemies, menus, scripting), VS2010 project files, CMake for Linux/macOS, and prebuilt 32-bit Windows dependencies in `HPL2/dependencies.zip`. Version string 1.0.3.
- **dima424658/HPL2Engine** (the repo you linked) is the *AMFP* flavour of HPL2 (its file headers say "Amnesia: A Machine For Pigs"), modernised to CMake 3.21 / C++17 / SDL2 / `find_package` dependencies. Only 34 source files differ from the official AMFP engine. It has no game code, so it's useful as a build-system reference, not as the base.
- **FrictionalGames/AmnesiaTheDarkDescent** is the sibling engine. It's close but not identical (~270 files differ, mostly licence headers), so we'll use the AMFP repo, not this one.

So the strongest route is a **source port**. We build our own `aamfp.exe` from source, add VR directly inside the renderer and player code, and run it against the Epic game data. Binary hooking (the VorpX / head-tracking-mod style) stays as a fallback only.

| | Source port (recommended) | Binary hook into the Epic exe |
|---|---|---|
| Stereo rendering | Render each eye natively, correct asymmetric projection | Must hijack the frame twice from outside. Fragile. |
| Motion controls | Rewrite interaction states to use hand poses | Very hard: input is mouse-delta based deep inside the game |
| UI in VR | Redirect GUI sets to a texture or world quad | Very hard |
| Maintenance | Normal C++ development, debuggable | Byte signatures break on any patch |
| Risk | Getting the 2020 source to build and match the Epic data | Ceiling is "head-look plus stereo", which VorpX already gives |

### Prior art worth borrowing

**itsloopyo/amnesia-the-dark-descent-headtracking** (an ASI hook for The Dark Descent) has one key lesson that carries straight over. In HPL2, `cCamera::GetViewMatrix` and `GetFrustum` feed **game logic** as well as rendering: the interaction ray, enemy "is seen by player", sanity drain, and the sound listener. Their hook therefore injects the head pose **only inside `cScene::Render`** and marks the camera dirty afterwards. In our port we do the same thing, cleanly: the **body camera** stays the gameplay camera, and per-eye **render cameras** are derived from it. We then decide case by case which gameplay systems should follow the head (the listener and "enemy sees player" should; the movement direction shouldn't).

---

## 2. What the engine looks like (the parts VR touches)

All paths are in the AMFP repo.

| Area | Where | What it means for VR |
|---|---|---|
| Main loop | `HPL2/core/sources/engine/Engine.cpp` (~L460–600) | Fixed-step logic (`PreUpdate/Update/PostUpdate`), then render, then `SwapBuffers`. We insert pose prediction (`WaitGetPoses` / `xrWaitFrame`) before render and frame submit before swap. |
| Scene render | `scene/Scene.cpp` `cScene::Render` | Already **loops over multiple viewports**, each with its own camera, renderer and render target. Two eye viewports render into two eye framebuffers. |
| Deferred renderer | `graphics/RendererDeferred.cpp` (3.9k lines), `graphics/Renderer.cpp` | G-buffer, SSAO, edge smoothing, lights, fog, translucency, reflections. All buffers are sized from `mvScreenSize`, so they must be sized to the HMD's per-eye resolution. |
| Projection | `math/Frustum.h` `SetupPerspectiveProj(mtxProj, mtxView, …)` | Accepts an arbitrary projection matrix, which is good. **But** the renderer rebuilds frustum corners from symmetric `FOV`/`Aspect` in several places (`Renderer.cpp:570`, `RendererDeferred.cpp:1077, 3195, 3404, 3488`). The Index's per-eye frustum is asymmetric. Fix: store left/right/top/bottom tangents in `cFrustum` and use them there. Fallback: render a symmetric frustum that encloses the eye frustum and crop with texture bounds. |
| GL context | `impl/LowLevelGraphicsSDL.cpp` | SDL2 window, **legacy/compatibility-profile OpenGL** (`glMatrixMode`, etc.) with GLSL shaders. OpenVR and OpenXR (`XR_KHR_opengl_enable`) both accept GL textures. RenderDoc does **not** capture compatibility-profile GL, so use apitrace or Nsight Graphics for frame debugging. |
| Player camera | `Game/src/game/LuxPlayer.cpp` | FOV and aspect multipliers, pitch limits ±70°, roll, head-spin on damage, camera effects. All of these must be neutralised or reinterpreted in VR. |
| Script camera control | `LuxScriptHandler.cpp` (273 script functions) | `FadePlayerFOVMulTo`, `FadePlayerAspectMulTo`, `FadePlayerRollTo`, `FadePlayerPitchTo`, `StartPlayerLookAt`, `MovePlayerHeadPos`, `StartScreenShake`, `GivePlayerDamage(…abSpinHead…)`. In VR these must never rotate the view against the head. Map them to body yaw, fades, or no-ops. |
| Interaction | `LuxPlayerState_Interact{Grab,Lever,Push,SwingDoor,Wheel,Slide,RotateBase}.cpp` | Driven by mouse deltas and a camera-relative hold point (e.g. Grab `vBasePos = offset + (0,0,-depth)`). In VR these are driven by the controller pose, which is the core of "real" motion controls. |
| Interaction ray | `LuxMapHelper::GetClosestEntity` (from camera pos plus forward) | Re-source the ray from the hand (or the head, as a gaze option). |
| Lantern / hands | `LuxPlayerHands.cpp`, `LuxHandObject_LightSource.cpp`, `LuxHandObject_Melee.cpp` | Attach the lantern model to the left/right controller. |
| HUD / menus | `cGuiSet`, `LuxPlayer::DrawHud`, `LuxMainMenu*`, `LuxInventory`, `LuxJournal` | 2D GUI is drawn screen-space. The engine already has **3D GUI sets** (`cScene::Render3DGui`), so menus can go on a world-space panel. |
| Input | `LuxInputHandler.cpp`, `impl/LowLevelInputSDL.cpp`, gamepad SDL2/XInput | Add a VR input device. SteamVR Input action manifest with Index bindings. |
| FBX | `impl/MeshLoaderFBX.cpp`, registered in `LowLevelResourcesSDL.cpp:76` | The 2012 FBX SDK no longer exists, so the official repo won't compile as-is. The Epic install has **255 `.FBX` files** (mostly character animations, e.g. `entities/character/gent/animations/gent_idle.FBX`) next to 368 `.anm` and 4,123 `.msh` files, which look like the converted copies the engine loads. So the loader can't simply be dropped: Phase 0 checks that every `.FBX` has a cached `.anm`/`.msh`. If some don't, we convert them once with a current FBX SDK or Assimp. |
| Dependencies | `HPL2/dependencies.zip` → `lib/win32/*` | Prebuilt **32-bit only** (Newton, OpenAL, SDL2, GLEW, DevIL, AngelScript, Theora, Cg…). Target **Win32 first**. OpenVR ships a Win32 `openvr_api.dll`. 64-bit is a later, optional step. |

---

## 3. Which repos to use, and how

### Use

| Repo | Verdict | Role |
|---|---|---|
| **FrictionalGames/AmnesiaAMachineForPigs** | **Base of the project** | Fork it; all VR work happens here. |
| **dima424658/HPL2Engine** | Reference | Crib its modern CMake/C++17/SDL2 setup when porting the VS2010 build to VS2022. |
| **bethington/ghidra-mcp** | **Use as MCP** (secondary) | Free Ghidra 12.x with Java 21, 209 tools, headless mode. Used to (a) diff the Epic `aamfp.exe` against our source build to confirm it's the same 1.0.3 code and spot any Epic-only changes, and (b) power the fallback hook route if the source build hits a wall. |
| **itsloopyo/amnesia-the-dark-descent-headtracking** | Reference | The render-only head-injection pattern, and HPL2 `cCamera` layout notes. |
| **rehan-remade/universal-modder** | **Use as Claude Code plugin** (skills plus `um` CLI, not its fal MCP) | `game-recon` / `um scan` to fingerprint the Epic install, `um backup` for saves, `game-automation` / `um win shot` to launch the game and screenshot for verification, and the knowledge base for writing up what we learn. Its fal asset-generation MCP isn't needed. |
| **trevaintdead/ai-game-modding-guides** | Reading only | Guides 14 (choosing a route), 16 (ownership, sync, rendering) and 13 (RE and the law) back up the choices here. Not a tool. |

### Skip (and why)

| Repo | Why not |
|---|---|
| **HexRaysSA/ida-mcp** | Needs a paid IDA 9.4+ with idalib. Ghidra-MCP covers what we need. Worth adding only if you already own IDA Pro/Home. |
| **morluto/rea** | A broad "reverse engineer anything" MCP (Hopper/Ghidra, JS, .NET, web). Overlaps with ghidra-mcp, and its strengths (Electron, web, Mac apps) don't apply. One native RE server is enough. |
| **icsharpcode/ILSpy** | .NET decompiler. AMFP is native C++, with no managed code. |
| **SamboyCoding/Cpp2IL** | Unity IL2CPP tool. Not Unity. |
| **boykopovar/AnyPS5**, **yuriolive/PortPS5** | PS5 dump-to-PC relinkers. Unrelated to a PC OpenGL game. |

### Tools not in your list that I'd add

- **OpenVR SDK** (ValveSoftware/openvr) or the **OpenXR SDK / loader**. This is the actual VR API (see question 3).
- **apitrace** (legacy-GL frame capture) and/or **NVIDIA Nsight Graphics**, for debugging the stereo render path.
- **Visual Studio 2022** with the v143 toolset, building Win32.

---

## 4. Phased plan

Every phase ends with something you can run and check.

### Phase 0: recon and safety (short)
- Epic install found at `C:\Program Files\Epic Games\AmnesiaAMFP`. `aamfp.exe` is 32-bit, dated October 2020, SDL2 + OpenGL, no Steam or EOS libraries, no `.pdb`. That matches the 2020 source release.
- Check that every one of the 255 `.FBX` files has a cached `.anm`/`.msh` counterpart.
- Back up saves and config (`Documents/Amnesia/Pig` or similar).
- Use ghidra-mcp to compare the Epic `aamfp.exe` with the 1.0.3 source: same functions, and any Epic Online Services or launcher hooks?

**Exit:** we know the source matches the data you own.

### Phase 1: vanilla source build ("our exe plays the game")
- Fork the AMFP repo. Port the build to VS2022 Win32 (or CMake modelled on dima424658's), using `dependencies.zip`.
- Stub the FBX loader if Phase 0 shows every `.FBX` is cached, otherwise convert the missing ones. Remove Steam/achievement bits that don't build.
- Run against the Epic data folder (our exe dropped next to the original, or `-dir` style resource paths). Play from the menu to the first save.

**Exit:** an unmodified-behaviour build runs the full game. *This is the biggest risk in the whole project, so it goes first.*

### Phase 2: stereo rendering and head tracking (seated, gamepad or keyboard)
- VR runtime init (OpenVR `VRApplication_Scene` or an OpenXR session with a GL binding), and a per-eye swapchain at the HMD's recommended size.
- Two eye viewports in `cScene` with render targets set to the eye textures. Renderer buffers sized to the eye resolution.
- Make `cFrustum` and the renderer asymmetric-projection aware (Section 2). Set world scale so 1 unit = 1 m (verify against door heights).
- Pose loop: predicted HMD pose → per-eye view = body camera × head pose × eye offset. The gameplay camera stays on the body (the head-tracking-mod lesson).
- Mirror the left eye to the desktop window. Submit, then swap.
- Neutralise FOV/aspect multipliers, roll, and screen shake (shake becomes a subtle effect, not a camera move).

**Exit:** walk through the game with a gamepad, seeing it in correct-scale 3D at 90 Hz (or 80 Hz).

### Phase 3: body, locomotion, comfort
- Decouple head yaw from body yaw. Thumbstick movement relative to head or hand (configurable). Snap or smooth turn.
- Room-scale: HMD floor-plane offset drives the character body, with collision. Seated or standing height calibration. Real crouch maps to the game's crouch.
- Script camera control in VR (`StartPlayerLookAt`, `FadePlayerPitchTo`, head spin): fade, rotate body yaw only, or ignore.
- Comfort pass on post-effects (radial blur, image trail, insanity effects), with per-effect toggles.

**Exit:** full playthrough is possible and comfortable.

### Phase 4: UI in VR
- Render 2D GUI sets (menus, inventory, journal, notes, hints, subtitles, load screens) to a texture shown on a world-space panel (or an OpenVR overlay).
- Crosshair becomes the controller ray / reticle at the hit depth. The head-locked centre crosshair and its "hand" interact icon must be gone (the tester's Phase 2 feedback).
- Laser-pointer menu interaction.

**Exit:** no flat-screen-only moments remain.

### Phase 5: motion controls (the "true VR" part)
- SteamVR Input action manifest with default **Index** bindings (grip, trigger, thumbstick, A/B), plus haptics.
- The interaction ray comes from the hand. `InteractGrab` follows the controller position/rotation instead of a camera-relative point plus mouse.
- Levers, wheels, swing doors, sliders and push states move with hand motion (project hand delta onto the constraint axis), replacing mouse deltas.
- Lantern held in a chosen hand, toggled by a gesture or button. The flat game's camera-attached arm/lantern model must be hidden in VR (the tester's Phase 2 feedback); the lantern follows the controller instead.
- Simple visible hands models, which change based on what you are doing (when equiping a lantern, a model of a hand holding a lantern should be visible).

**Exit:** you open doors, turn valves and carry objects with your hands.

### Phase 6: polish and packaging
- Performance (the deferred renderer twice per frame, CPU-side submission). Optional 64-bit build. Optional single-pass stereo later.
- An installer or launcher that drops the VR exe next to the Epic data. **Never redistribute game data.** The code is GPLv3, so the fork stays public if shared.
- A field note in universal-modder's knowledge base, if you want to share.

---

## 5. Main risks

1. **Source build parity** (Phase 1): 2020-era source, missing FBX SDK, VS2010 project, 32-bit libs. Mitigation: dima424658's modernised build as a guide, and stub FBX once the cached-asset check passes.
2. **Epic data vs the 1.0.3 source:** possible small differences. Mitigation: Phase 0 Ghidra diff.
3. **Asymmetric projection in the deferred renderer:** known spots listed in Section 2. The enclose-and-crop fallback exists.
4. **Performance:** two full deferred passes, legacy GL. The engine is from 2013, so a modern GPU should cope at Index resolution, but SSAO and edge-smooth may need lowering.
5. **Scripted sequences** that grab the camera (look-ats, head spins, cutscenes). These need a VR-aware policy in each case.

---

## 6. Open questions for the tester

1. ~~Where is the game installed?~~ Answered: `C:\Program Files\Epic Games\AmnesiaAMFP`.
2. **How far should v1 go?** Head tracking plus stereo with gamepad (Phases 0–3), or full motion controls (through Phase 5)? *Recommendation: plan for full motion controls, ship a playable seated build at the end of Phase 3.*
3. **Which VR API?** OpenVR is simplest for SteamVR/Index and is guaranteed to work with a 32-bit GL app. OpenXR is the modern standard and would also cover Quest and other headsets. *Recommendation: OpenVR first, behind a thin interface so OpenXR can be added later.*
4. **Personal use or public release?** Public means a public GPLv3 fork and an installer.
5. **Dev setup:** is Visual Studio 2022 (with C++) on the gaming PC, and do you own IDA Pro? This decides whether ida-mcp is worth it.
