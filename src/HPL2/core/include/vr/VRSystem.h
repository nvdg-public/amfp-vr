/*
 * Copyright © 2011-2020 Frictional Games
 *
 * This file is part of Amnesia: A Machine For Pigs.
 *
 * Amnesia: A Machine For Pigs is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * Amnesia: A Machine For Pigs is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Amnesia: A Machine For Pigs.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef HPL_VR_SYSTEM_H
#define HPL_VR_SYSTEM_H

#include "math/MathTypes.h"

namespace hpl {

	class cFrustum;
	class cCamera;
	class cConfigFile;
	class cMesh;
	class cGraphics;
	class cResources;

	//------------------------------------------------------------

	enum eVREye
	{
		eVREye_Left,
		eVREye_Right,
		eVREye_LastEnum
	};

	//------------------------------------------------------------

	/** Which part of the 2D screen GUI is being rendered (see cGuiSet::Render). */
	enum eVRGuiPass
	{
		eVRGuiPass_All,			// not in VR, or 3D GUI
		eVRGuiPass_Panel,		// menus/HUD into the GUI panel texture
		eVRGuiPass_FullScreen,	// full-screen fades/flashes/images into each eye
		eVRGuiPass_LastEnum
	};

	//------------------------------------------------------------

	enum eVRHand
	{
		eVRHand_Left,
		eVRHand_Right,
		eVRHand_LastEnum
	};

	//------------------------------------------------------------

	/** Controller buttons (SteamVR Input actions, see run\vr_actions.json; Index defaults in
		run\vr_bindings_knuckles.json, rebindable in SteamVR). The Turn/Menu ones are derived from the turn stick. */
	enum eVRButton
	{
		eVRButton_Interact,		// right trigger: interact, click in menus
		eVRButton_Grab,			// right grip: pick up / hold objects
		eVRButton_Jump,			// right B
		eVRButton_Crouch,		// right A
		eVRButton_Recenter,		// right stick click
		eVRButton_Run,			// left stick click
		eVRButton_Lantern,		// left trigger
		eVRButton_Journal,		// left A
		eVRButton_Menu,			// left B: pause menu / back
		eVRButton_Rotate,		// left grip: rotate a held object
		eVRButton_InteractLeft,	// left trigger: interact with the left hand (when it is the active hand)
		eVRButton_TurnUp,		// turn stick pushed up (scroll in menus)
		eVRButton_TurnDown,		// turn stick pushed down
		eVRButton_LastEnum
	};

	//------------------------------------------------------------

	/**
	 * OpenVR (SteamVR) output for the engine.
	 *
	 * Enabled at runtime with the environment variable HPL_VR=1 (flat mode otherwise, or if SteamVR/HMD
	 * is missing). When active, the "screen" the engine renders to is an eye-sized offscreen
	 * framebuffer per eye (see cLowLevelGraphicsSDL), the scene is rendered once per eye with a frustum
	 * built from the body camera + HMD pose, and both eyes are submitted to the compositor.
	 *
	 * Each eye is rendered with a symmetric frustum that encloses the HMD's asymmetric eye frustum, and
	 * submitted with texture bounds that crop it to the real eye frustum (the HPL2 deferred renderer
	 * assumes symmetric projections in several places).
	 */
	class cVRSystem
	{
	public:
		static cVRSystem* Get();

		/** Starts OpenVR if HPL_VR=1. Returns true if VR is active. Call before the window is created. */
		bool Init();
		/** Creates the eye framebuffers. Call with the GL context current (after glewInit). */
		bool CreateEyeBuffers();
		void Shutdown();

		bool IsActive() const { return mbActive; }

		/** Size of each eye framebuffer = the engine's virtual screen size in VR. */
		cVector2l GetRenderSize() const { return mvRenderSize; }
		/** Size requested for the desktop mirror window. */
		cVector2l GetMirrorSize() const { return mvMirrorSize; }
		/** Whether the renderer's screen-space edge smoothing runs in VR. */
		bool GetEdgeSmooth() const { return mbEdgeSmooth; }

		unsigned int GetEyeFramebuffer(eVREye aEye) const { return mvEyeFramebuffer[aEye]; }

		/** Blocks until the compositor wants the next frame and fetches predicted poses. */
		void WaitGetPoses();
		/** Submits both eye textures to the compositor. */
		void Submit();
		/** Copies the left eye (cropped) into the window's default framebuffer. */
		void BlitMirror(const cVector2l& avWindowSize);
		/** Makes the current head pose the new seated origin. */
		void Recenter();

		/** Fills afFrustum with the render frustum for an eye, from the body camera and the HMD pose. */
		void SetupEyeFrustum(eVREye aEye, cCamera *apBodyCamera, cFrustum *apFrustum);

		/** Eye currently being rendered (-1 = none). Per-frame data that is consumed while rendering
			(e.g. GUI draw lists) must survive until the last eye. */
		void SetRenderingEye(int alEye){ mlRenderingEye = alEye; }
		int GetRenderingEye() const { return mlRenderingEye; }
		/** True after the last eye, and outside the VR render passes (-1). The GUI panel pass uses -2. */
		bool IsRenderingLastEye() const { return mlRenderingEye == -1 || mlRenderingEye == eVREye_LastEnum-1; }

		//// GUI panel: menus/HUD on a world-locked SteamVR overlay //////////////////
		/** Framebuffer of the GUI panel texture (screen-sized). */
		unsigned int GetGuiPanelFramebuffer() const { return mlGuiPanelFramebuffer; }
		void SetGuiPass(eVRGuiPass aPass){ mGuiPass = aPass; }
		eVRGuiPass GetGuiPass() const { return mGuiPass; }
		/** Hands the freshly rendered GUI panel texture to the overlay (call once per frame). */
		void SubmitGuiPanel();

		/** Menu mode (a menu, journal, inventory or load screen is open; set by cUpdater::SetContainer): entering it
			places the panel in front of the head and enables gaze pointing. Outside it, the panel (HUD: hints,
			subtitles, messages) follows the head lazily if Menu Follow="Lazy". */
		void SetGuiPanelMenuMode(bool abX);
		bool GetGuiPanelMenuMode() const { return mbMenuMode; }
		/** Puts the panel straight ahead of the head (yaw only), at the configured distance and height. */
		void PlaceGuiPanelInFront();
		/** Menu mode: where the head's gaze ray hits the panel, in screen pixels (= panel texture pixels, y down).
			False if gaze pointing is off or the gaze misses the panel. */
		bool GetGazePointer(cVector2l &avPos) const;

		/** Logic step length, used to interpolate the body between logic steps. */
		void SetLogicStepSize(float afStep){ mfLogicStep = afStep; }

		/** Where straight ahead (the eye's forward axis) falls in the eye image, 0..1 from the bottom-left. */
		cVector2f GetStraightAheadUV(eVREye aEye) const;

		/** World transform of the head as rendered this frame (player camera: body x head pose; set by the first
			eye's SetupEyeFrustum). Used to place head-relative 3D GUI such as the reticle. */
		const cMatrixf& GetRenderHeadWorld() const { return m_mtxRenderHeadWorld; }

		/** Head pose in seated tracking space (identity if no valid pose). */
		const cMatrixf& GetHeadPose() const { return m_mtxHead; }

		/** The loaded vr_settings.cfg (never NULL once VR is active), for game-side settings. */
		cConfigFile* GetConfig() const { return mpConfig; }

		//// SteamVR keyboard for text fields (Menu Keyboard) ////////////////
		/** Opens the SteamVR keyboard for apOwner (e.g. a text box); typing replaces its text. False if not available. */
		bool ShowKeyboard(void *apOwner, const tWString &asText, int alMaxLength);
		/** Closes the keyboard if apOwner opened it. */
		void HideKeyboard(void *apOwner);
		/** Input for apOwner since the last call: 0 = none, 1 = text changed, 2 = Done pressed. asText gets the full text. */
		int GetKeyboardInput(void *apOwner, tWString &asText);

		//// Player head: the game's camera follows the head (Phase 3) ////////////////
		/** Head yaw and pitch (radians, camera conventions) and head position, in seated space. */
		float GetHeadYaw() const;
		float GetHeadPitch() const;
		cVector3f GetHeadPosition() const { return m_mtxHead.GetTranslation(); }

		/**
		 * The game's player camera turns with the head: its yaw = body yaw + the head yaw the game has applied
		 * ("consumed"), and its position includes the head offset (cCamera::SetHeadOffset). For this camera the eyes
		 * are rendered from the camera's base position and yaw minus the consumed head yaw, so head motion is not
		 * applied twice.
		 *  Anchor: seated-space XZ point where the body is (room-scale: the body follows the head, the anchor follows).
		 *  YAdd: added to the head height (crouch compensation). Head height above the recentered height is limited.
		 */
		void SetPlayerCamera(cCamera *apCamera){ mpPlayerCamera = apCamera; }
		void SetConsumedHeadYaw(float afYaw){ mfConsumedHeadYaw = afYaw; }
		float GetConsumedHeadYaw() const { return mfConsumedHeadYaw; }
		void SetHeadAnchor(const cVector3f& avAnchor){ mvHeadAnchor = avAnchor; }
		const cVector3f& GetHeadAnchor() const { return mvHeadAnchor; }
		void SetHeadYAdd(float afY){ mfHeadYAdd = afY; }
		/** Head position relative to the body, in seated space (anchor removed, height limited, YAdd added). */
		cVector3f GetPlayerHeadOffset() const;
		/** Number of recenters so far (the game resets its head state when this changes). */
		int GetRecenterCount() const { return mlRecenterCount; }

		//// Controllers (Phase 5) /////////////////////////////////////////////
		/** True when SteamVR Input is set up (action manifest loaded). Buttons/sticks/poses are only valid then. */
		bool HasControllerInput() const { return mbInputActive; }
		/** Button state, updated once per rendered frame. */
		bool GetButton(eVRButton aButton) const;
		/** Raw trigger / grip of one physical hand (whatever the active hand is). */
		bool GetHandTrigger(eVRHand aHand) const { return aHand==eVRHand_Left ? mvButtons[eVRButton_InteractLeft] : mvButtons[eVRButton_Interact]; }
		bool GetHandGrip(eVRHand aHand) const { return aHand==eVRHand_Left ? mvButtons[eVRButton_Rotate] : mvButtons[eVRButton_Grab]; }
		/** The hand that interacts (pointer ray, haptics, hand-driven grab/doors): the game switches it to the hand
			that pressed its grip or trigger. GetButton(Interact/Grab) then report that hand's trigger/grip. Right after
			a switch both read as released for a moment, so the focus is taken from the new hand before the press. */
		void SetActiveHand(eVRHand aHand);
		eVRHand GetConfiguredPointerHand() const { return mPointerHand; }
		static const char* GetButtonName(eVRButton aButton);
		/** Move stick (left) and turn stick (right), -1..1, x = right, y = forward. Zero within the dead zone. */
		cVector2f GetMoveAxis() const { return mvMoveAxis; }
		cVector2f GetTurnAxis() const { return mvTurnAxis; }
		/** Hand poses in seated space. Aim = pointing pose (tip, -Z forward), Grip = holding pose. False if not tracked. */
		bool GetHandAimPose(eVRHand aHand, cMatrixf &a_mtxPose) const;
		bool GetHandGripPose(eVRHand aHand, cMatrixf &a_mtxPose) const;
		/** Finger curl 0 (straight) .. 1 (curled into the palm) from SteamVR skeletal input (Index finger tracking;
			other controllers get SteamVR's estimate from their buttons). Finger: 0 thumb, 1 index, 2 middle, 3 ring, 4 pinky.
			False when no skeletal data is available for that hand. */
		bool GetFingerCurls(eVRHand aHand, float *apCurls) const;
		/** Which hand points (interaction ray, menu laser) and which holds the lantern (vr_settings.cfg Input Hand). */
		eVRHand GetPointerHand() const { return mActiveHand; }
		eVRHand GetLanternHand() const { return mPointerHand==eVRHand_Right ? eVRHand_Left : eVRHand_Right; }
		/** Input Pointer="Hand" (default) or "Head": whether the hand or the gaze aims interaction and menus. */
		bool UsesHandPointer() const;
		/** Vibrates a controller. */
		void TriggerHaptic(eVRHand aHand, float afSeconds, float afFrequency, float afAmplitude);

		/** Seated space to world, for the player camera as the game logic has it (latest logic step). */
		cMatrixf GetSeatedToWorld(cCamera *apCamera) const;
		/** Seated space to world as rendered this frame (interpolated body; valid after WaitGetPoses). */
		const cMatrixf& GetRenderSeatedToWorld() const { return m_mtxRenderSeatedToWorld; }

		/** Controller model (SteamVR render model of the controller in that hand, e.g. the Index controller) as an engine
			mesh with a diffuse material. Loaded in the background: NULL until ready (call every frame). The mesh is
			kept for the whole session (an extra user count), so world entities using it can come and go. */
		cMesh* GetControllerMesh(eVRHand aHand, cGraphics *apGraphics, cResources *apResources);
		/** Pose of the controller device itself (render model space) in seated space. */
		bool GetControllerModelPose(eVRHand aHand, cMatrixf &a_mtxPose) const;

		/** Folder with our own VR assets (hand models): vr_assets next to the exe. */
		static tWString GetAssetDir();

		/** Called at the end of WaitGetPoses, before the eyes are rendered (fresh poses): place hand-held things. */
		typedef void (*tPreRenderCallback)(void *apUserData);
		void SetPreRenderCallback(tPreRenderCallback apFunc, void *apUserData){ mpPreRenderCallback = apFunc; mpPreRenderUserData = apUserData; }

	private:
		// Controllers
		void InitInput();
		void UpdateInput();
		bool mbInputActive;
		bool mvButtons[eVRButton_LastEnum];
		cVector2f mvMoveAxis, mvTurnAxis;
		bool mvAimValid[eVRHand_LastEnum], mvGripValid[eVRHand_LastEnum];
		cMatrixf m_mtxAim[eVRHand_LastEnum], m_mtxGrip[eVRHand_LastEnum];
		eVRHand mPointerHand;		// configured (vr_settings.cfg Input Hand); the lantern goes in the other hand
		eVRHand mActiveHand;		// the hand interacting right now
		unsigned long mlActiveHandSwitchTime;
		bool mbHandPointer;
		float mfStickDeadZone;
		unsigned long long mlActionSet;
		unsigned long long mvButtonAction[eVRButton_LastEnum];
		unsigned long long mlMoveAction, mlTurnAction;
		unsigned long long mlInteractPullAction, mlInteractLeftPullAction;	// analog trigger pull: a soft press counts as Interact
		float mfTriggerThreshold;
		bool mbLeftHanded;	// Input LeftHanded: sticks, stick clicks and face buttons swap hands (UpdateInput)
		unsigned long long mvAimAction[eVRHand_LastEnum], mvGripAction[eVRHand_LastEnum], mvHapticAction[eVRHand_LastEnum];
		unsigned long long mvSkeletonAction[eVRHand_LastEnum];	// 0 if the manifest has no skeleton action
		bool mvFingerValid[eVRHand_LastEnum];
		float mvFingerCurl[eVRHand_LastEnum][5];
		bool mbRecenterButtonDown;
		// Controller models
		int mvDeviceIndex[eVRHand_LastEnum];
		bool mvModelPoseValid[eVRHand_LastEnum];
		cMatrixf m_mtxModelPose[eVRHand_LastEnum];
		tString mvModelName[eVRHand_LastEnum];
		void *mvRenderModel[eVRHand_LastEnum];	// vr::RenderModel_t* while loading
		cMesh *mvControllerMesh[eVRHand_LastEnum];
		bool mvModelFailed[eVRHand_LastEnum];
		cMatrixf m_mtxRenderSeatedToWorld;
		void UpdateRenderBody();
		void GetBodyTransform(cCamera *apBodyCamera, cVector3f &avPos, float &afYaw) const;
		int mlRenderBodyFrame;
		tPreRenderCallback mpPreRenderCallback;
		void *mpPreRenderUserData;

		cVRSystem();

		bool mbActive;
		bool mbHasPose;
		cVector2l mvRenderSize;
		cVector2l mvMirrorSize;

		// Settings (vr_settings.cfg next to the exe, overridable with HPL_VR_* environment variables)
		void LoadSettings();
		void CaptureIfRequested();
		float mfRenderScale;
		int mlRefreshRateHint;
		float mfMenuDistance, mfMenuWidth, mfMenuHeight;
		int mlRecenterKey;
		bool mbMirrorEnabled;
		bool mbEdgeSmooth;
		int mlRenderingEye;

		cConfigFile *mpConfig;
		cCamera *mpPlayerCamera;
		float mfConsumedHeadYaw;
		cVector3f mvHeadAnchor;
		float mfHeadYAdd;
		float mfHeadMaxUp;
		int mlRecenterCount;

		bool CreateGuiPanel();
		void UpdateGuiPanelTransform();
		void UpdateGuiPanelFollowAndGaze();
		cMatrixf m_mtxGuiPanel;		// seated space
		bool mbMenuMode;
		bool mbMenuFollowLazy;
		float mfMenuFollowAngle;	// radians
		bool mbGazePointer;
		bool mbGazeHit;
		cVector2l mvGazePos;
		bool mbPlacePanelOnNextPose;
		eVRGuiPass mGuiPass;
		unsigned int mlGuiPanelFramebuffer, mlGuiPanelTexture;
		unsigned long long mlGuiOverlay;
		bool mbKeyboard;		// Menu Keyboard
		void *mpKeyboardOwner;
		bool mbKeyboardOpen;
		int mlKeyboardInput;	// pending GetKeyboardInput result
		tWString msKeyboardText;

		// Symmetric enclosing frustum and the crop to the real eye frustum.
		float mfTanX, mfTanY;
		float mvBounds[eVREye_LastEnum][4]; // uMin, vMin, uMax, vMax
		float mvEyeTan[eVREye_LastEnum][4]; // rendered frustum sides: left, right, bottom, top (GL convention)
		bool mbAsymmetricProjection;

		cMatrixf m_mtxEyeToHead[eVREye_LastEnum];
		cMatrixf m_mtxHead;
		cMatrixf m_mtxRenderHeadWorld;

		unsigned int mvEyeFramebuffer[eVREye_LastEnum];
		unsigned int mvEyeTexture[eVREye_LastEnum];
		unsigned int mvEyeDepth[eVREye_LastEnum];

		int mlFrameCount;

		// Body interpolation: the game moves the camera at the logic rate (60 Hz), the HMD wants 90-120 Hz.
		// Render the body between the previous and the latest logic position/yaw (one step behind, smooth).
		void UpdateBodyInterpolation(cCamera *apCamera, cVector3f &avPos, float &afYaw);
		cCamera *mpInterpCamera;
		cVector3f mvBodyPrevPos, mvBodyCurPos;
		float mfBodyPrevYaw, mfBodyCurYaw;
		double mfBodyChangeTime;
		float mfLogicStep;
		cVector3f mvRenderBodyPos;
		float mfRenderBodyYaw;

		// Timing (logged every 500 frames)
		double mfWaitEnd, mfLastSubmit;
		double mfIntervalSum, mfRenderSum, mfWaitSum;
		int mlTimingFrames;
	};

	//------------------------------------------------------------

};
#endif // HPL_VR_SYSTEM_H
