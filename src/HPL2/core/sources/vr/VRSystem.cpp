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

#include "vr/VRSystem.h"

#include "impl/LowLevelGraphicsSDL.h"

#include "system/LowLevelSystem.h"
#include "system/String.h"
#include "math/Math.h"
#include "math/Frustum.h"
#include "scene/Camera.h"
#include "resources/ConfigFile.h"
#include "system/Platform.h"

#include <stdlib.h>
#include <stdio.h>
#include <vector>
#ifdef WIN32
#include <windows.h>
#endif

#ifdef HPL_VR
	#include <openvr.h>
	#pragma comment(lib, "openvr_api.lib")
#endif

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// HELPERS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

#ifdef HPL_VR
	// Shared with VRSystemInput.cpp
	vr::IVRSystem *gpHmd = NULL;
	vr::TrackedDevicePose_t gvPoses[vr::k_unMaxTrackedDeviceCount];

	static cMatrixf ToMatrix(const vr::HmdMatrix34_t& m)
	{
		return cMatrixf(m.m[0][0], m.m[0][1], m.m[0][2], m.m[0][3],
						m.m[1][0], m.m[1][1], m.m[1][2], m.m[1][3],
						m.m[2][0], m.m[2][1], m.m[2][2], m.m[2][3],
						0, 0, 0, 1);
	}
#endif

	static bool EnvFlag(const char *asName)
	{
		const char *pVal = getenv(asName);
		return pVal && (pVal[0]=='1' || pVal[0]=='y' || pVal[0]=='Y' || pVal[0]=='t' || pVal[0]=='T');
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cVRSystem* cVRSystem::Get()
	{
		static cVRSystem gVRSystem;
		return &gVRSystem;
	}

	cVRSystem::cVRSystem()
	{
		mbActive = false;
		mbHasPose = false;
		mvRenderSize = cVector2l(0,0);
		mvMirrorSize = cVector2l(1280,720);
		mfRenderScale = 1.0f;
		mlRefreshRateHint = 0;
		mfMenuDistance = 2.0f; mfMenuWidth = 2.0f; mfMenuHeight = 0.0f;
		mlRecenterKey = 0;
		mbMirrorEnabled = true;
		mlRenderingEye = -1;
		mbAsymmetricProjection = true;
		mbEdgeSmooth = false;
		mGuiPass = eVRGuiPass_All;
		mlGuiPanelFramebuffer = mlGuiPanelTexture = 0;
		mlGuiOverlay = 0;
		for(int i=0; i<eVREye_LastEnum; ++i)
		{
			mvEyeTan[i][0] = mvEyeTan[i][2] = -1;
			mvEyeTan[i][1] = mvEyeTan[i][3] = 1;
		}
		mfTanX = mfTanY = 1;
		for(int i=0; i<eVREye_LastEnum; ++i)
		{
			mvEyeFramebuffer[i] = mvEyeTexture[i] = mvEyeDepth[i] = 0;
			m_mtxEyeToHead[i] = cMatrixf::Identity;
			mvBounds[i][0] = mvBounds[i][1] = 0; mvBounds[i][2] = mvBounds[i][3] = 1;
		}
		m_mtxHead = cMatrixf::Identity;
		m_mtxRenderHeadWorld = cMatrixf::Identity;
		m_mtxGuiPanel = cMatrixf::Identity;
		mbMenuMode = false;
		mbMenuFollowLazy = true;
		mfMenuFollowAngle = cMath::ToRad(40.0f);
		mbGazePointer = true;
		mbKeyboard = true;
		mpKeyboardOwner = NULL;
		mbKeyboardOpen = false;
		mlKeyboardInput = 0;
		mbGazeHit = false;
		mvGazePos = cVector2l(0,0);
		mbPlacePanelOnNextPose = false;
		mlFrameCount = 0;

		mpInterpCamera = NULL;
		mvBodyPrevPos = mvBodyCurPos = mvRenderBodyPos = 0;
		mfBodyPrevYaw = mfBodyCurYaw = mfRenderBodyYaw = 0;
		mfBodyChangeTime = 0;
		mfLogicStep = 1.0f/60.0f;

		mpConfig = NULL;
		mpPlayerCamera = NULL;
		mfConsumedHeadYaw = 0;
		mvHeadAnchor = 0;
		mfHeadYAdd = 0;
		mfHeadMaxUp = 0.15f;
		mlRecenterCount = 0;

		mbInputActive = false;
		for(int i=0; i<eVRButton_LastEnum; ++i){ mvButtons[i] = false; mvButtonAction[i] = 0; }
		mvMoveAxis = mvTurnAxis = 0;
		for(int i=0; i<eVRHand_LastEnum; ++i)
		{
			mvAimValid[i] = mvGripValid[i] = false;
			m_mtxAim[i] = m_mtxGrip[i] = cMatrixf::Identity;
			mvAimAction[i] = mvGripAction[i] = mvHapticAction[i] = mvSkeletonAction[i] = 0;
			mvFingerValid[i] = false;
			for(int f=0; f<5; ++f) mvFingerCurl[i][f] = 0;
			mvDeviceIndex[i] = -1;
			mvModelPoseValid[i] = false;
			m_mtxModelPose[i] = cMatrixf::Identity;
			mvRenderModel[i] = NULL;
			mvControllerMesh[i] = NULL;
			mvModelFailed[i] = false;
		}
		mPointerHand = mActiveHand = eVRHand_Right;
		mlActiveHandSwitchTime = 0;
		mbHandPointer = true;
		mfStickDeadZone = 0.15f;
		mlActionSet = mlMoveAction = mlTurnAction = mlInteractPullAction = mlInteractLeftPullAction = 0;
		mfTriggerThreshold = 0.5f;
		mbRecenterButtonDown = false;
		m_mtxRenderSeatedToWorld = cMatrixf::Identity;
		mlRenderBodyFrame = -1;
		mpPreRenderCallback = NULL;
		mpPreRenderUserData = NULL;

		mfWaitEnd = mfLastSubmit = 0;
		mfIntervalSum = mfRenderSum = mfWaitSum = 0;
		mlTimingFrames = 0;
	}

	//-----------------------------------------------------------------------

	static double SecondsNow()
	{
#ifdef WIN32
		static LARGE_INTEGER freq = {0};
		if(freq.QuadPart==0) QueryPerformanceFrequency(&freq);
		LARGE_INTEGER t; QueryPerformanceCounter(&t);
		return (double)t.QuadPart / (double)freq.QuadPart;
#else
		return (double)cPlatform::GetApplicationTime() / 1000.0;
#endif
	}

	void cVRSystem::UpdateBodyInterpolation(cCamera *apCamera, cVector3f &avPos, float &afYaw)
	{
		double fNow = SecondsNow();
		if(apCamera != mpInterpCamera)
		{
			mpInterpCamera = apCamera;
			mvBodyPrevPos = mvBodyCurPos = avPos;
			mfBodyPrevYaw = mfBodyCurYaw = afYaw;
			mfBodyChangeTime = fNow;
			return;
		}

		// A new logic step moved the camera: interpolate from the previous logic step to this one. (Starting from
		// where we rendered last froze every other frame at 120 Hz render / 60 Hz logic: walking judder.)
		if(avPos != mvBodyCurPos || afYaw != mfBodyCurYaw)
		{
			mvBodyPrevPos = mvBodyCurPos;
			mfBodyPrevYaw = mfBodyCurYaw;
			mvBodyCurPos = avPos;
			mfBodyCurYaw = afYaw;
			mfBodyChangeTime = fNow;

			// Teleports and big (snap) turns snap
			float fYawJump = cMath::Abs(cMath::GetAngleDistanceRad(mfBodyPrevYaw, mfBodyCurYaw));
			if(cMath::Vector3Dist(mvBodyPrevPos, mvBodyCurPos) > 1.0f || fYawJump > cMath::ToRad(15.0f))
			{
				mvBodyPrevPos = mvBodyCurPos; mfBodyPrevYaw = mfBodyCurYaw;
			}
		}

		float fT = mfLogicStep > 0 ? (float)((fNow - mfBodyChangeTime) / mfLogicStep) : 1.0f;
		if(fT > 1) fT = 1;
		if(fT < 0) fT = 0;

		float fYawDelta = mfBodyCurYaw - mfBodyPrevYaw;
		while(fYawDelta > kPif) fYawDelta -= k2Pif;
		while(fYawDelta < -kPif) fYawDelta += k2Pif;

		avPos = mvBodyPrevPos + (mvBodyCurPos - mvBodyPrevPos) * fT;
		afYaw = mfBodyPrevYaw + fYawDelta * fT;
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	bool cVRSystem::Init()
	{
		if(mbActive) return true;
		if(EnvFlag("HPL_VR")==false) return false;

#ifndef HPL_VR
		Warning("HPL_VR=1 but this build has no VR support, running flat.\n");
		return false;
#else
		Log("Initializing VR (OpenVR)\n");

		if(vr::VR_IsHmdPresent()==false)
		{
			Warning(" No HMD found, running flat.\n");
			return false;
		}

		vr::EVRInitError initError = vr::VRInitError_None;
		gpHmd = vr::VR_Init(&initError, vr::VRApplication_Scene);
		if(initError != vr::VRInitError_None || gpHmd==NULL)
		{
			Warning(" VR_Init failed: %s, running flat.\n", vr::VR_GetVRInitErrorAsEnglishDescription(initError));
			gpHmd = NULL;
			return false;
		}
		if(vr::VRCompositor()==NULL)
		{
			Warning(" No compositor, running flat.\n");
			vr::VR_Shutdown(); gpHmd = NULL;
			return false;
		}
		vr::VRCompositor()->SetTrackingSpace(vr::TrackingUniverseSeated);

		////////////////////////////
		// Per eye frustum: build a symmetric frustum that encloses both eyes, crop with texture bounds
		uint32_t lRecW=0, lRecH=0;
		gpHmd->GetRecommendedRenderTargetSize(&lRecW, &lRecH);

		float vRaw[2][4]; // left, right, top, bottom (tangents, y down)
		float fMaxX = 0, fMaxY = 0;
		float fSpanX = 0, fSpanY = 0;
		for(int i=0; i<eVREye_LastEnum; ++i)
		{
			vr::EVREye eye = i==0 ? vr::Eye_Left : vr::Eye_Right;
			gpHmd->GetProjectionRaw(eye, &vRaw[i][0], &vRaw[i][1], &vRaw[i][2], &vRaw[i][3]);
			m_mtxEyeToHead[i] = ToMatrix(gpHmd->GetEyeToHeadTransform(eye));

			fMaxX = cMath::Max(fMaxX, cMath::Max(fabs(vRaw[i][0]), fabs(vRaw[i][1])));
			fMaxY = cMath::Max(fMaxY, cMath::Max(fabs(vRaw[i][2]), fabs(vRaw[i][3])));
			fSpanX = cMath::Max(fSpanX, vRaw[i][1] - vRaw[i][0]);
			fSpanY = cMath::Max(fSpanY, vRaw[i][3] - vRaw[i][2]);

			Log(" Eye %d raw projection: left %.4f right %.4f top %.4f bottom %.4f\n", i, vRaw[i][0], vRaw[i][1], vRaw[i][2], vRaw[i][3]);
			Log(" Eye %d to head: %s\n", i, m_mtxEyeToHead[i].ToString().c_str());
		}
		mfTanX = fMaxX;
		mfTanY = fMaxY;

		LoadSettings();
		InitInput();

		float fPixelsPerTanY = (float)lRecH / fSpanY * mfRenderScale;
		if(mbAsymmetricProjection)
		{
			////////////////////////////
			// Off-axis frustum per eye, exactly the HMD's: no wasted pixels, no cropping.
			// GL convention: left/bottom negative, right/top positive (OpenVR's raw top/bottom are y-down).
			for(int i=0; i<eVREye_LastEnum; ++i)
			{
				mvEyeTan[i][0] = vRaw[i][0];
				mvEyeTan[i][1] = vRaw[i][1];
				mvEyeTan[i][2] = -vRaw[i][3];
				mvEyeTan[i][3] = -vRaw[i][2];
				mvBounds[i][0] = mvBounds[i][1] = 0;
				mvBounds[i][2] = mvBounds[i][3] = 1;
			}
			mvRenderSize.y = (int)(fPixelsPerTanY * fSpanY + 0.5f);
			mvRenderSize.x = (int)(fPixelsPerTanY * fSpanX + 0.5f);
		}
		else
		{
			////////////////////////////
			// Fallback: symmetric frustum enclosing both eyes, cropped with texture bounds
			for(int i=0; i<eVREye_LastEnum; ++i)
			{
				mvEyeTan[i][0] = -fMaxX; mvEyeTan[i][1] = fMaxX;
				mvEyeTan[i][2] = -fMaxY; mvEyeTan[i][3] = fMaxY;
				mvBounds[i][0] = (vRaw[i][0] + fMaxX) / (2*fMaxX); // uMin
				mvBounds[i][2] = (vRaw[i][1] + fMaxX) / (2*fMaxX); // uMax
				mvBounds[i][1] = (vRaw[i][2] + fMaxY) / (2*fMaxY); // vMin (top)
				mvBounds[i][3] = (vRaw[i][3] + fMaxY) / (2*fMaxY); // vMax (bottom)
			}
			// Keep the recommended pixel density inside the cropped area, and square pixels.
			mvRenderSize.y = (int)(fPixelsPerTanY * 2*fMaxY + 0.5f);
			mvRenderSize.x = (int)(fPixelsPerTanY * 2*fMaxX + 0.5f);
		}

		Log(" Recommended per-eye size %dx%d, render size %dx%d (scale %.2f), %s projection\n",
			lRecW, lRecH, mvRenderSize.x, mvRenderSize.y, mfRenderScale, mbAsymmetricProjection ? "asymmetric (exact)" : "symmetric (enclosing, cropped)");
		Log(" Bounds left u %.3f-%.3f v %.3f-%.3f, right u %.3f-%.3f v %.3f-%.3f\n",
			mvBounds[0][0], mvBounds[0][2], mvBounds[0][1], mvBounds[0][3], mvBounds[1][0], mvBounds[1][2], mvBounds[1][1], mvBounds[1][3]);

		float fRefresh = gpHmd->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_DisplayFrequency_Float);
		Log(" Display frequency: %.1f Hz\n", fRefresh);
		if(mlRefreshRateHint > 0 && fabs(fRefresh - (float)mlRefreshRateHint) > 0.5f)
		{
			Log(" Note: vr_settings.cfg asks for %d Hz but the headset runs at %.0f Hz. Change it in SteamVR > Settings > Video.\n",
				mlRefreshRateHint, fRefresh);
		}

		mbActive = true;
		return true;
#endif
	}

	//-----------------------------------------------------------------------

	static int KeyNameToVirtualKey(const tString& asName)
	{
#ifdef WIN32
		tString sKey = cString::ToUpperCase(asName);
		if(sKey.size() >= 2 && sKey[0]=='F')
		{
			int lNum = atoi(sKey.c_str()+1);
			if(lNum >= 1 && lNum <= 12) return VK_F1 + lNum - 1;
		}
		if(sKey.size()==1 && ((sKey[0]>='A' && sKey[0]<='Z') || (sKey[0]>='0' && sKey[0]<='9'))) return sKey[0];
		Warning("VR: unknown RecenterKey '%s', using F9\n", asName.c_str());
		return VK_F9;
#else
		return 0;
#endif
	}

	//-----------------------------------------------------------------------

	static void WriteTextureBMP(unsigned int alTexture, const char *asFile)
	{
		glBindTexture(GL_TEXTURE_2D, alTexture);
		int w=0, h=0;
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w);
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h);
		if(w<=0 || h<=0) { glBindTexture(GL_TEXTURE_2D, 0); return; }

		std::vector<unsigned char> vPixels((size_t)w*h*4);
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, &vPixels[0]);
		glBindTexture(GL_TEXTURE_2D, 0);

		FILE *pFile = fopen(asFile, "wb");
		if(pFile==NULL) { Warning("VR capture: can't write '%s'\n", asFile); return; }

		// 32-bit BMP keeps the alpha channel (GUI panel)
		unsigned int lDataSize = w * h * 4;
		unsigned char vHeader[54] = {'B','M'};
		*(unsigned int*)&vHeader[2] = 54 + lDataSize;
		*(unsigned int*)&vHeader[10] = 54;
		*(unsigned int*)&vHeader[14] = 40;
		*(int*)&vHeader[18] = w;
		*(int*)&vHeader[22] = h; // bottom-up rows, same as GL
		*(unsigned short*)&vHeader[26] = 1;
		*(unsigned short*)&vHeader[28] = 32;
		*(unsigned int*)&vHeader[34] = lDataSize;
		fwrite(vHeader, 1, 54, pFile);
		for(size_t i=0; i<(size_t)w*h; ++i)
		{
			unsigned char t = vPixels[i*4+0]; vPixels[i*4+0] = vPixels[i*4+2]; vPixels[i*4+2] = t; // RGBA -> BGRA
		}
		fwrite(&vPixels[0], 1, lDataSize, pFile);
		fclose(pFile);
		Log("VR capture: wrote %s (%dx%d)\n", asFile, w, h);
	}

	/**
	 * Debug: HPL_VR_CAPTURE_DIR=<dir> and HPL_VR_CAPTURE_FRAMES=<n>,<n>,... write, for those frames:
	 * eye0/eye1_frame<n>.bmp (the eye images as rendered), panel_frame<n>.bmp (the GUI panel texture, with
	 * alpha) and compositor_frame<n>.bmp (SteamVR's mirror of the left eye, including the overlay).
	 * For checking per-eye differences and the GUI panel without the headset.
	 */
	void cVRSystem::CaptureIfRequested()
	{
		const char *pDir = getenv("HPL_VR_CAPTURE_DIR");
		const char *pFrames = getenv("HPL_VR_CAPTURE_FRAMES");
		if(pDir==NULL || pFrames==NULL) return;

		// Is this frame in the list?
		bool bCapture = false;
		tString sFrames = pFrames;
		tStringVec vFrames;
		cString::GetStringVec(sFrames, vFrames, NULL);
		for(size_t i=0; i<vFrames.size(); ++i) if(cString::ToInt(vFrames[i].c_str(), -1) == mlFrameCount) bCapture = true;
		if(bCapture==false) return;

		char sName[1024];
		for(int eye=0; eye<eVREye_LastEnum; ++eye)
		{
			sprintf(sName, "%s\\eye%d_frame%d.bmp", pDir, eye, mlFrameCount);
			WriteTextureBMP(mvEyeTexture[eye], sName);
		}
		if(mlGuiPanelTexture)
		{
			sprintf(sName, "%s\\panel_frame%d.bmp", pDir, mlFrameCount);
			WriteTextureBMP(mlGuiPanelTexture, sName);
		}

#ifdef HPL_VR
		vr::glUInt_t lMirrorTex = 0;
		vr::glSharedTextureHandle_t pShared = NULL;
		if(vr::VRCompositor()->GetMirrorTextureGL(vr::Eye_Left, &lMirrorTex, &pShared) == vr::VRCompositorError_None)
		{
			vr::VRCompositor()->LockGLSharedTextureForAccess(pShared);
			sprintf(sName, "%s\\compositor_frame%d.bmp", pDir, mlFrameCount);
			WriteTextureBMP(lMirrorTex, sName);
			vr::VRCompositor()->UnlockGLSharedTextureForAccess(pShared);
			vr::VRCompositor()->ReleaseSharedGLTexture(lMirrorTex, pShared);
		}
		else
		{
			Warning("VR capture: no compositor mirror texture\n");
		}
#endif
	}

	//-----------------------------------------------------------------------

	tWString cVRSystem::GetAssetDir()
	{
#ifdef WIN32
		wchar_t sExe[MAX_PATH];
		if(GetModuleFileNameW(NULL, sExe, MAX_PATH) > 0)
			return cString::GetFilePathW(sExe) + _W("vr_assets");
#endif
		return _W("vr_assets");
	}

	//-----------------------------------------------------------------------

	void cVRSystem::LoadSettings()
	{
		////////////////////////////////
		// File: next to the exe, or HPL_VR_CONFIG (the exe's folder: see also GetAssetDir)
		tWString sFile;
		const char *pCfg = getenv("HPL_VR_CONFIG");
		if(pCfg && pCfg[0])
		{
			sFile = cString::To16Char(pCfg);
		}
		else
		{
#ifdef WIN32
			wchar_t sExe[MAX_PATH];
			if(GetModuleFileNameW(NULL, sExe, MAX_PATH) > 0)
				sFile = cString::GetFilePathW(sExe) + _W("vr_settings.cfg");
#endif
			if(sFile == _W("")) sFile = _W("vr_settings.cfg");
		}

		if(mpConfig) delete mpConfig;
		mpConfig = new cConfigFile(sFile);
		cConfigFile &cfg = *mpConfig;
		bool bLoaded = cPlatform::FileExists(sFile) && cfg.Load();
		Log(" VR settings: '%s' %s\n", cString::To8Char(sFile).c_str(), bLoaded ? "loaded" : "not found, using defaults");

		mfRenderScale = cfg.GetFloat("Render", "Scale", 1.0f);
		mbAsymmetricProjection = cString::ToLowerCase(cfg.GetString("Render", "Projection", "Asymmetric")) != "symmetric";
		mbEdgeSmooth = cfg.GetBool("Render", "EdgeSmooth", false);
		mlRefreshRateHint = cfg.GetInt("Display", "RefreshRateHint", 0);
		mfMenuDistance = cfg.GetFloat("Menu", "Distance", 2.0f);
		mfMenuWidth = cfg.GetFloat("Menu", "Width", 2.0f);
		mfMenuHeight = cfg.GetFloat("Menu", "Height", 0.0f);
		mbMenuFollowLazy = cString::ToLowerCase(cfg.GetString("Menu", "Follow", "Lazy")) != "fixed";
		mfMenuFollowAngle = cMath::ToRad(cMath::Clamp(cfg.GetFloat("Menu", "FollowAngle", 40.0f), 10.0f, 180.0f));
		mbGazePointer = cfg.GetBool("Menu", "GazePointer", true);
		mbKeyboard = cfg.GetBool("Menu", "Keyboard", true);
		mlRecenterKey = KeyNameToVirtualKey(cfg.GetString("Input", "RecenterKey", "F9"));
		mbMirrorEnabled = cfg.GetBool("Mirror", "Enabled", true);
		mfHeadMaxUp = cfg.GetFloat("Body", "MaxHeadRise", 0.15f);
		mvMirrorSize.x = cfg.GetInt("Mirror", "Width", 1280);
		mvMirrorSize.y = cfg.GetInt("Mirror", "Height", 720);
		mPointerHand = cString::ToLowerCase(cfg.GetString("Input", "Hand", "Right")) == "left" ? eVRHand_Left : eVRHand_Right;
		mActiveHand = mPointerHand;
		mbHandPointer = cString::ToLowerCase(cfg.GetString("Input", "Pointer", "Hand")) != "head";
		mfStickDeadZone = cMath::Clamp(cfg.GetFloat("Input", "StickDeadZone", 0.15f), 0.0f, 0.8f);
		mfTriggerThreshold = cMath::Clamp(cfg.GetFloat("Input", "TriggerThreshold", 0.5f), 0.05f, 1.0f);

		////////////////////////////////
		// Environment overrides (for tests and quick experiments)
		const char *pScale = getenv("HPL_VR_SCALE");
		if(pScale && atof(pScale) > 0) mfRenderScale = (float)atof(pScale);

		const char *pMirror = getenv("HPL_VR_MIRROR");
		if(pMirror)
		{
			int w=0,h=0;
			if(sscanf(pMirror, "%dx%d", &w, &h)==2 && w>0 && h>0) mvMirrorSize = cVector2l(w,h);
		}

		////////////////////////////////
		// Sanity
		mfRenderScale = cMath::Clamp(mfRenderScale, 0.3f, 2.0f);
		mfMenuDistance = cMath::Clamp(mfMenuDistance, 0.5f, 10.0f);
		mfMenuWidth = cMath::Clamp(mfMenuWidth, 0.2f, 10.0f);
		if(mvMirrorSize.x < 160 || mvMirrorSize.y < 120) mvMirrorSize = cVector2l(1280,720);

		Log(" VR settings: render scale %.2f, refresh hint %d, menu %.2f m away %.2f m wide (height %.2f), recenter key 0x%x, mirror %s %dx%d\n",
			mfRenderScale, mlRefreshRateHint, mfMenuDistance, mfMenuWidth, mfMenuHeight, mlRecenterKey,
			mbMirrorEnabled ? "on" : "off", mvMirrorSize.x, mvMirrorSize.y);
		Log(" VR settings: projection %s, edge smooth %s\n", mbAsymmetricProjection ? "asymmetric" : "symmetric", mbEdgeSmooth ? "on" : "off");
	}

	//-----------------------------------------------------------------------

	bool cVRSystem::CreateEyeBuffers()
	{
		if(mbActive==false) return false;

		for(int i=0; i<eVREye_LastEnum; ++i)
		{
			glGenTextures(1, &mvEyeTexture[i]);
			glBindTexture(GL_TEXTURE_2D, mvEyeTexture[i]);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, mvRenderSize.x, mvRenderSize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			glBindTexture(GL_TEXTURE_2D, 0);

			glGenRenderbuffersEXT(1, &mvEyeDepth[i]);
			glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, mvEyeDepth[i]);
			glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH24_STENCIL8_EXT, mvRenderSize.x, mvRenderSize.y);
			glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);

			glGenFramebuffersEXT(1, &mvEyeFramebuffer[i]);
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, mvEyeFramebuffer[i]);
			glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, mvEyeTexture[i], 0);
			glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, mvEyeDepth[i]);
			glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, mvEyeDepth[i]);
			GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
			glClearColor(0,0,0,1);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

			if(status != GL_FRAMEBUFFER_COMPLETE_EXT)
			{
				Error("VR eye framebuffer %d incomplete (0x%x)\n", i, status);
				return false;
			}
		}
		Log(" VR eye framebuffers created: %d, %d (%dx%d)\n", mvEyeFramebuffer[0], mvEyeFramebuffer[1], mvRenderSize.x, mvRenderSize.y);

		if(CreateGuiPanel()==false) Warning(" VR: no GUI panel overlay, menus will not be visible in the headset\n");
		return true;
	}

	//-----------------------------------------------------------------------

	bool cVRSystem::CreateGuiPanel()
	{
#ifdef HPL_VR
		////////////////////////////
		// Texture + framebuffer, same size as the engine's (eye-sized) screen so the GUI renders unchanged
		glGenTextures(1, &mlGuiPanelTexture);
		glBindTexture(GL_TEXTURE_2D, mlGuiPanelTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, mvRenderSize.x, mvRenderSize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glBindTexture(GL_TEXTURE_2D, 0);

		glGenFramebuffersEXT(1, &mlGuiPanelFramebuffer);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, mlGuiPanelFramebuffer);
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, mlGuiPanelTexture, 0);
		// GUI does not use depth, but share the left eye's depth/stencil so stencil-based clipping works
		glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, mvEyeDepth[0]);
		glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, mvEyeDepth[0]);
		GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
		glClearColor(0,0,0,0);
		glClear(GL_COLOR_BUFFER_BIT);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		if(status != GL_FRAMEBUFFER_COMPLETE_EXT)
		{
			Error("VR GUI panel framebuffer incomplete (0x%x)\n", status);
			return false;
		}

		////////////////////////////
		// Overlay
		if(vr::VROverlay()==NULL) return false;
		vr::VROverlayHandle_t handle = vr::k_ulOverlayHandleInvalid;
		vr::EVROverlayError err = vr::VROverlay()->CreateOverlay("machineforpigsvr.gui", "Amnesia: A Machine for Pigs - menus", &handle);
		if(err != vr::VROverlayError_None)
		{
			Error("VR: CreateOverlay failed: %s\n", vr::VROverlay()->GetOverlayErrorNameFromEnum(err));
			return false;
		}
		mlGuiOverlay = handle;

		vr::VROverlay()->SetOverlayWidthInMeters(handle, mfMenuWidth);
		vr::VROverlay()->SetOverlaySortOrder(handle, 0);
		// (no texture-bounds flip: SteamVR already shows GL overlay textures upright; a flip made them upside down)
		UpdateGuiPanelTransform();
		vr::VROverlay()->ShowOverlay(handle);

		Log(" VR GUI panel: %.2f m wide (%.2f m tall), %.2f m ahead, %.2f m from eye height\n",
			mfMenuWidth, mfMenuWidth * mvRenderSize.y / mvRenderSize.x, mfMenuDistance, mfMenuHeight);
		return true;
#else
		return false;
#endif
	}

	//-----------------------------------------------------------------------

	void cVRSystem::UpdateGuiPanelTransform()
	{
#ifdef HPL_VR
		if(mlGuiOverlay==0) return;
		// Fixed in the seated universe (m_mtxGuiPanel; at start: straight ahead of the recentered head pose)
		if(m_mtxGuiPanel == cMatrixf::Identity) m_mtxGuiPanel.SetTranslation(cVector3f(0, mfMenuHeight, -mfMenuDistance));
		vr::HmdMatrix34_t mtx;
		for(int r=0; r<3; ++r) for(int c=0; c<4; ++c) mtx.m[r][c] = m_mtxGuiPanel.m[r][c];
		vr::VROverlay()->SetOverlayTransformAbsolute(mlGuiOverlay, vr::TrackingUniverseSeated, &mtx);
#endif
	}

	//-----------------------------------------------------------------------

	static cVector3f HeadFlatForward(const cMatrixf &a_mtxHead)
	{
		// Forward = -Z column of the head rotation, projected on the floor
		cVector3f vFwd(-a_mtxHead.m[0][2], 0, -a_mtxHead.m[2][2]);
		if(vFwd.SqrLength() < 1e-6f) return cVector3f(0,0,-1);
		vFwd.Normalize();
		return vFwd;
	}

	void cVRSystem::PlaceGuiPanelInFront()
	{
		cVector3f vFwd = HeadFlatForward(m_mtxHead);
		cVector3f vRight(-vFwd.z, 0, vFwd.x);
		cVector3f vHead = m_mtxHead.GetTranslation();
		cVector3f vPos(vHead.x + vFwd.x*mfMenuDistance, mfMenuHeight, vHead.z + vFwd.z*mfMenuDistance);

		// Columns: X = right, Y = up, Z = towards the player (-forward)
		m_mtxGuiPanel = cMatrixf(	vRight.x,	0,	-vFwd.x,	vPos.x,
									vRight.y,	1,	-vFwd.y,	vPos.y,
									vRight.z,	0,	-vFwd.z,	vPos.z,
									0,			0,	0,			1);
		UpdateGuiPanelTransform();
	}

	void cVRSystem::SetGuiPanelMenuMode(bool abX)
	{
		if(mbActive==false || abX == mbMenuMode) return;
		mbMenuMode = abX;
		if(mbMenuMode && mbHasPose) PlaceGuiPanelInFront();
	}

	bool cVRSystem::GetGazePointer(cVector2l &avPos) const
	{
		if(mbGazeHit==false) return false;
		avPos = mvGazePos;
		return true;
	}

	void cVRSystem::UpdateGuiPanelFollowAndGaze()
	{
		mbGazeHit = false;
		if(mlGuiOverlay==0 || mbHasPose==false) return;

		if(mbPlacePanelOnNextPose)
		{
			mbPlacePanelOnNextPose = false;
			PlaceGuiPanelInFront();
		}

		cVector3f vPanelPos = m_mtxGuiPanel.GetTranslation();
		cVector3f vPanelRight(m_mtxGuiPanel.m[0][0], m_mtxGuiPanel.m[1][0], m_mtxGuiPanel.m[2][0]);
		cVector3f vPanelUp(m_mtxGuiPanel.m[0][1], m_mtxGuiPanel.m[1][1], m_mtxGuiPanel.m[2][1]);
		cVector3f vPanelNormal(m_mtxGuiPanel.m[0][2], m_mtxGuiPanel.m[1][2], m_mtxGuiPanel.m[2][2]);

		////////////////////////////////
		// HUD: follow the head lazily, so hints and subtitles stay in view after turning around
		if(mbMenuMode==false)
		{
			if(mbMenuFollowLazy)
			{
				cVector3f vFwd = HeadFlatForward(m_mtxHead);
				float fCos = cMath::Vector3Dot(vFwd, vPanelNormal * -1.0f);
				if(fCos < cos(mfMenuFollowAngle)) PlaceGuiPanelInFront();
			}
			return;
		}

		////////////////////////////////
		// Menus: gaze ray against the panel plane
		if(mbGazePointer==false) return;
		// Pointer: the pointing hand's laser (if tracked), else the head's gaze
		cMatrixf mtxPointer = m_mtxHead;
		if(UsesHandPointer()) GetHandAimPose(mActiveHand, mtxPointer);
		cVector3f vOrigin = mtxPointer.GetTranslation();
		cVector3f vDir(-mtxPointer.m[0][2], -mtxPointer.m[1][2], -mtxPointer.m[2][2]);
		float fDenom = cMath::Vector3Dot(vDir, vPanelNormal);
		if(fDenom > -1e-4f) return; // looking away from the panel
		float fT = cMath::Vector3Dot(vPanelPos - vOrigin, vPanelNormal) / fDenom;
		if(fT <= 0) return;
		cVector3f vLocal = vOrigin + vDir*fT - vPanelPos;
		float fHeight = mfMenuWidth * (float)mvRenderSize.y / (float)mvRenderSize.x;
		float fU = cMath::Vector3Dot(vLocal, vPanelRight) / mfMenuWidth + 0.5f;
		float fV = 0.5f - cMath::Vector3Dot(vLocal, vPanelUp) / fHeight;
		if(fU < 0 || fU > 1 || fV < 0 || fV > 1) return;

		mvGazePos = cVector2l((int)(fU * mvRenderSize.x), (int)(fV * mvRenderSize.y));
		mbGazeHit = true;
	}

	//-----------------------------------------------------------------------

	void cVRSystem::SubmitGuiPanel()
	{
#ifdef HPL_VR
		if(mlGuiOverlay==0) return;
		vr::Texture_t tex = { (void*)(uintptr_t)mlGuiPanelTexture, vr::TextureType_OpenGL, vr::ColorSpace_Gamma };
		vr::EVROverlayError err = vr::VROverlay()->SetOverlayTexture(mlGuiOverlay, &tex);
		if(err != vr::VROverlayError_None && mlFrameCount < 5)
			Warning("VR: SetOverlayTexture failed: %s\n", vr::VROverlay()->GetOverlayErrorNameFromEnum(err));
#endif
	}

	//-----------------------------------------------------------------------

	void cVRSystem::Shutdown()
	{
		if(mbActive==false) return;
#ifdef HPL_VR
		for(int i=0; i<eVREye_LastEnum; ++i)
		{
			if(mvEyeFramebuffer[i]) glDeleteFramebuffersEXT(1, &mvEyeFramebuffer[i]);
			if(mvEyeDepth[i]) glDeleteRenderbuffersEXT(1, &mvEyeDepth[i]);
			if(mvEyeTexture[i]) glDeleteTextures(1, &mvEyeTexture[i]);
			mvEyeFramebuffer[i] = mvEyeDepth[i] = mvEyeTexture[i] = 0;
		}
		if(mlGuiOverlay && vr::VROverlay()) vr::VROverlay()->DestroyOverlay(mlGuiOverlay);
		mlGuiOverlay = 0;
		if(mlGuiPanelFramebuffer) glDeleteFramebuffersEXT(1, &mlGuiPanelFramebuffer);
		if(mlGuiPanelTexture) glDeleteTextures(1, &mlGuiPanelTexture);
		mlGuiPanelFramebuffer = mlGuiPanelTexture = 0;
		vr::VR_Shutdown();
		gpHmd = NULL;
#endif
		mbActive = false;
		Log("VR shut down\n");
	}

	//-----------------------------------------------------------------------

	bool cVRSystem::ShowKeyboard(void *apOwner, const tWString &asText, int alMaxLength)
	{
		if(mbActive==false || mbKeyboard==false) return false;
#ifdef HPL_VR
		if(vr::VROverlay()==NULL) return false;
		if(mbKeyboardOpen) vr::VROverlay()->HideKeyboard();

		// Starts empty: the typed text is collected from the key events (see WaitGetPoses)
		uint32_t lMax = alMaxLength > 0 ? (uint32_t)alMaxLength : 256;
		vr::EVROverlayError err = vr::VROverlay()->ShowKeyboard(vr::k_EGamepadTextInputModeNormal, vr::k_EGamepadTextInputLineModeSingleLine,
																0, "", lMax, "", 0);
		if(err != vr::VROverlayError_None)
		{
			Warning("VR: ShowKeyboard failed: %s\n", vr::VROverlay()->GetOverlayErrorNameFromEnum(err));
			return false;
		}
		mpKeyboardOwner = apOwner;
		mbKeyboardOpen = true;
		mlKeyboardInput = 0;
		msKeyboardText = _W("");
		return true;
#else
		return false;
#endif
	}

	void cVRSystem::HideKeyboard(void *apOwner)
	{
		if(apOwner != mpKeyboardOwner) return;
#ifdef HPL_VR
		if(mbKeyboardOpen && vr::VROverlay()) vr::VROverlay()->HideKeyboard();
#endif
		mbKeyboardOpen = false;
		mpKeyboardOwner = NULL;
		mlKeyboardInput = 0;
	}

	int cVRSystem::GetKeyboardInput(void *apOwner, tWString &asText)
	{
		if(apOwner != mpKeyboardOwner || mlKeyboardInput == 0) return 0;
		int lResult = mlKeyboardInput;
		mlKeyboardInput = 0;
		asText = msKeyboardText;
		if(lResult == 2 || mbKeyboardOpen==false) mpKeyboardOwner = NULL;
		return lResult;
	}

	//-----------------------------------------------------------------------

	void cVRSystem::WaitGetPoses()
	{
		if(mbActive==false) return;
#ifdef HPL_VR
		// Handle quit requests and the like from SteamVR
		vr::VREvent_t event;
		while(gpHmd->PollNextEvent(&event, sizeof(event)))
		{
			if(event.eventType == vr::VREvent_SeatedZeroPoseReset) Log("VR: seated zero pose reset\n");

			// SteamVR keyboard: keep its text for the text box that opened it
			// The current SteamVR keyboard leaves cNewInput empty and GetKeyboardText returns only the newest key
			// (checked in the headset 2026-10-10), so the text is built here key by key.
			if(mpKeyboardOwner && event.eventType == vr::VREvent_KeyboardCharInput)
			{
				char sInput[16] = "";
				memcpy(sInput, event.data.keyboard.cNewInput, 8);
				if(sInput[0] == 0) vr::VROverlay()->GetKeyboardText(sInput, sizeof(sInput));
				if(sInput[0] == '\b')
				{
					if(msKeyboardText.empty()==false) msKeyboardText.resize(msKeyboardText.size()-1);
				}
				else if(sInput[0] != 0x1b && sInput[0] != '\n' && sInput[0] != '\r' && sInput[0] != 0)	// skip arrow keys / Enter
				{
					msKeyboardText += cString::UTF8ToWChar(sInput);
				}
				if(mlKeyboardInput == 0) mlKeyboardInput = 1;
			}
			if(mpKeyboardOwner && event.eventType == vr::VREvent_KeyboardDone)
				mlKeyboardInput = 2;
			if(event.eventType == vr::VREvent_KeyboardClosed || event.eventType == vr::VREvent_KeyboardClosed_Global)
				mbKeyboardOpen = false;
		}

		double fBeforeWait = SecondsNow();
		vr::VRCompositor()->WaitGetPoses(gvPoses, vr::k_unMaxTrackedDeviceCount, NULL, 0);
		mfWaitEnd = SecondsNow();
		mfWaitSum += mfWaitEnd - fBeforeWait;

		const vr::TrackedDevicePose_t &hmd = gvPoses[vr::k_unTrackedDeviceIndex_Hmd];
		mbHasPose = hmd.bPoseIsValid;
		if(mbHasPose) m_mtxHead = ToMatrix(hmd.mDeviceToAbsoluteTracking);

		// Debug: HPL_VR_FIXED_POSE=1 ignores head tracking (identity head pose), for reproducible captures
		static bool bFixedPose = EnvFlag("HPL_VR_FIXED_POSE");
		if(bFixedPose) m_mtxHead = cMatrixf::Identity;

		// Recenter key (vr_settings.cfg, default F9). Works without window focus: the player is in the headset.
#ifdef WIN32
		static bool bKeyDown = false;
		bool bDown = mlRecenterKey != 0 && (GetAsyncKeyState(mlRecenterKey) & 0x8000) != 0;
		if(bDown && !bKeyDown) Recenter();
		bKeyDown = bDown;
#endif

		// Controllers: buttons, sticks and hand poses for this frame
		UpdateInput();
		if(mvButtons[eVRButton_Recenter] && !mbRecenterButtonDown) Recenter();
		mbRecenterButtonDown = mvButtons[eVRButton_Recenter];

		UpdateGuiPanelFollowAndGaze();

		// Body as rendered this frame, then let the game place hand-held objects with the fresh poses
		UpdateRenderBody();
		if(mpPreRenderCallback) mpPreRenderCallback(mpPreRenderUserData);

		if(mlFrameCount == 0) Log("VR: first poses received (HMD pose valid: %d)\n", mbHasPose ? 1 : 0);
#endif
	}

	//-----------------------------------------------------------------------

	void cVRSystem::Submit()
	{
		if(mbActive==false) return;
#ifdef HPL_VR
		for(int i=0; i<eVREye_LastEnum; ++i)
		{
			vr::Texture_t tex = { (void*)(uintptr_t)mvEyeTexture[i], vr::TextureType_OpenGL, vr::ColorSpace_Gamma };
			vr::VRTextureBounds_t bounds;
			bounds.uMin = mvBounds[i][0];
			bounds.uMax = mvBounds[i][2];
			// Bounds are in image space (v=0 = top of the image); the compositor handles GL's bottom-up rows.
			bounds.vMin = mvBounds[i][1];
			bounds.vMax = mvBounds[i][3];
			vr::EVRCompositorError err = vr::VRCompositor()->Submit(i==0 ? vr::Eye_Left : vr::Eye_Right, &tex, &bounds);
			if(err != vr::VRCompositorError_None && (mlFrameCount < 5 || mlFrameCount % 600 == 0))
				Warning("VR: Submit eye %d failed: %d\n", i, (int)err);
		}
		vr::VRCompositor()->PostPresentHandoff();

		// Our own timing: CPU time from the end of WaitGetPoses to here (both eyes rendered and submitted)
		double fNow = SecondsNow();
		mfRenderSum += fNow - mfWaitEnd;
		if(mfLastSubmit > 0) mfIntervalSum += fNow - mfLastSubmit;
		mfLastSubmit = fNow;
		++mlTimingFrames;

		if(mlFrameCount == 0) Log("VR: first frame submitted\n");
		CaptureIfRequested();
		++mlFrameCount;

		// Log timing every 500 frames: our averages plus the compositor's view
		if(mlFrameCount % 500 == 0)
		{
			vr::Compositor_CumulativeStats stats;
			vr::VRCompositor()->GetCumulativeStats(&stats, sizeof(stats));
			vr::Compositor_FrameTiming timing;
			timing.m_nSize = sizeof(vr::Compositor_FrameTiming);
			bool bTiming = vr::VRCompositor()->GetFrameTiming(&timing, 0);

			float fN = (float)(mlTimingFrames > 0 ? mlTimingFrames : 1);
			Log("VR timing: frame %d | app: interval %.2f ms, render+submit %.2f ms, wait %.2f ms | "
				"compositor: app gpu %.2f ms, presents/frame %d, reprojection 0x%x | total presented %u dropped %u reprojected %u\n",
				mlFrameCount,
				(float)(mfIntervalSum*1000/fN), (float)(mfRenderSum*1000/fN), (float)(mfWaitSum*1000/fN),
				bTiming ? timing.m_flPreSubmitGpuMs + timing.m_flPostSubmitGpuMs : -1.0f,
				bTiming ? (int)timing.m_nNumFramePresents : -1, bTiming ? timing.m_nReprojectionFlags : 0,
				stats.m_nNumFramePresents, stats.m_nNumDroppedFrames, stats.m_nNumReprojectedFrames);
			mfIntervalSum = mfRenderSum = mfWaitSum = 0;
			mlTimingFrames = 0;
		}
#endif
	}

	//-----------------------------------------------------------------------

	void cVRSystem::BlitMirror(const cVector2l& avWindowSize)
	{
		if(mbActive==false) return;
		if(mbMirrorEnabled==false)
		{
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
			glDisable(GL_SCISSOR_TEST);
			glViewport(0, 0, avWindowSize.x, avWindowSize.y);
			glClearColor(0,0,0,1);
			glClear(GL_COLOR_BUFFER_BIT);
			return;
		}

		// Left eye, cropped to its real frustum, fitted into the window
		int lSrcX0 = (int)(mvBounds[0][0] * mvRenderSize.x);
		int lSrcX1 = (int)(mvBounds[0][2] * mvRenderSize.x);
		int lSrcY0 = (int)((1.0f - mvBounds[0][3]) * mvRenderSize.y);
		int lSrcY1 = (int)((1.0f - mvBounds[0][1]) * mvRenderSize.y);

		float fSrcAspect = (float)(lSrcX1-lSrcX0) / (float)(lSrcY1-lSrcY0);
		int lDstW = avWindowSize.x, lDstH = (int)(avWindowSize.x / fSrcAspect);
		if(lDstH > avWindowSize.y) { lDstH = avWindowSize.y; lDstW = (int)(avWindowSize.y * fSrcAspect); }
		int lDstX = (avWindowSize.x - lDstW)/2, lDstY = (avWindowSize.y - lDstH)/2;

		glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, mvEyeFramebuffer[0]);
		glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, 0);
		glDisable(GL_SCISSOR_TEST);
		glViewport(0, 0, avWindowSize.x, avWindowSize.y);
		glClearColor(0,0,0,1);
		glClear(GL_COLOR_BUFFER_BIT);
		glBlitFramebufferEXT(lSrcX0, lSrcY0, lSrcX1, lSrcY1, lDstX, lDstY, lDstX+lDstW, lDstY+lDstH, GL_COLOR_BUFFER_BIT, GL_LINEAR);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	}

	//-----------------------------------------------------------------------

	void cVRSystem::Recenter()
	{
		if(mbActive==false) return;
#ifdef HPL_VR
		if(vr::VRChaperone()) vr::VRChaperone()->ResetZeroPose(vr::TrackingUniverseSeated);
		mvHeadAnchor = 0;
		++mlRecenterCount;
		mbPlacePanelOnNextPose = true;
		Log("VR: recentered\n");
#endif
	}

	//-----------------------------------------------------------------------

	void cVRSystem::SetupEyeFrustum(eVREye aEye, cCamera *apBodyCamera, cFrustum *apFrustum)
	{
		cMatrixf mtxHead = m_mtxHead;
		if(apBodyCamera == mpPlayerCamera)
		{
			cVector3f vHead = m_mtxHead.GetTranslation();
			vHead.y = cMath::Min(vHead.y, mfHeadMaxUp) + mfHeadYAdd;
			mtxHead.SetTranslation(vHead);
		}

		// Body once per frame (on the first eye; the player camera's is already done in WaitGetPoses), both eyes
		// use the same body transform
		if(aEye == eVREye_Left && (apBodyCamera != mpPlayerCamera || mlRenderBodyFrame != mlFrameCount))
		{
			cVector3f vPos; float fYaw;
			GetBodyTransform(apBodyCamera, vPos, fYaw);
			UpdateBodyInterpolation(apBodyCamera, vPos, fYaw);
			mvRenderBodyPos = vPos;
			mfRenderBodyYaw = fYaw;
		}
		cMatrixf mtxBody = cMath::MatrixRotateY(mfRenderBodyYaw);
		mtxBody.SetTranslation(mvRenderBodyPos);
		if(aEye == eVREye_Left) m_mtxRenderHeadWorld = cMath::MatrixMul(mtxBody, mtxHead);

		cMatrixf mtxEyeWorld = cMath::MatrixMul(mtxBody, cMath::MatrixMul(mtxHead, m_mtxEyeToHead[aEye]));
		cMatrixf mtxView = cMath::MatrixInverse(mtxEyeWorld);

		float fNear = apBodyCamera->GetNearClipPlane();
		float fFar = apBodyCamera->GetFarClipPlane();
		// Off-axis projection (glFrustum) from the eye's side tangents; finite far plane like cCamera::GetFrustum
		float fL = mvEyeTan[aEye][0], fR = mvEyeTan[aEye][1], fB = mvEyeTan[aEye][2], fT = mvEyeTan[aEye][3];
		float fZA = -(fFar + fNear) / (fFar - fNear);
		float fZB = -(2.0f*fFar*fNear) / (fFar - fNear);
		cMatrixf mtxProj(	2.0f/(fR-fL),	0,				(fR+fL)/(fR-fL),	0,
							0,				2.0f/(fT-fB),	(fT+fB)/(fT-fB),	0,
							0,				0,				fZA,				fZB,
							0,				0,				-1,					0);

		apFrustum->SetupAsymmetricPerspectiveProj(mtxProj, mtxView, fFar, fNear, fL, fR, fB, fT, mtxEyeWorld.GetTranslation());
	}

	//-----------------------------------------------------------------------

	void cVRSystem::GetBodyTransform(cCamera *apBodyCamera, cVector3f &avPos, float &afYaw) const
	{
		////////////////////////////////
		// Body: position + yaw of the gameplay camera. Pitch, roll and FOV changes of the gameplay
		// camera are ignored, the head pose gives the view direction.
		if(apBodyCamera->GetRotateMode() == eCameraRotateMode_EulerAngles)
		{
			afYaw = apBodyCamera->GetYaw();
		}
		else
		{
			// Matrix mode: keep only the yaw of the camera's forward direction
			cVector3f vFwd = apBodyCamera->GetForward();
			afYaw = atan2(-vFwd.x, -vFwd.z);
		}
		avPos = apBodyCamera->GetBasePosition();

		if(apBodyCamera == mpPlayerCamera)
		{
			// The game's camera already turned with the head: take the applied head yaw out again (body yaw),
			// and render from the play space origin (body position minus the anchor), so that head motion
			// is applied once, from the latest pose. Interpolating the origin keeps room-scale steps smooth.
			afYaw -= mfConsumedHeadYaw;
			avPos -= cMath::MatrixMul(cMath::MatrixRotateY(afYaw), cVector3f(mvHeadAnchor.x, 0, mvHeadAnchor.z));
		}
	}

	void cVRSystem::UpdateRenderBody()
	{
		if(mpPlayerCamera==NULL) return;
		cVector3f vPos; float fYaw;
		GetBodyTransform(mpPlayerCamera, vPos, fYaw);
		UpdateBodyInterpolation(mpPlayerCamera, vPos, fYaw);
		mvRenderBodyPos = vPos;
		mfRenderBodyYaw = fYaw;
		mlRenderBodyFrame = mlFrameCount;

		// Seated space -> world: body, plus the crouch compensation the head gets
		m_mtxRenderSeatedToWorld = cMath::MatrixRotateY(fYaw);
		m_mtxRenderSeatedToWorld.SetTranslation(vPos + cVector3f(0, mfHeadYAdd, 0));
	}

	cMatrixf cVRSystem::GetSeatedToWorld(cCamera *apCamera) const
	{
		cVector3f vPos; float fYaw;
		GetBodyTransform(apCamera, vPos, fYaw);
		cMatrixf mtx = cMath::MatrixRotateY(fYaw);
		mtx.SetTranslation(vPos + cVector3f(0, apCamera==mpPlayerCamera ? mfHeadYAdd : 0, 0));
		return mtx;
	}

	//-----------------------------------------------------------------------

	float cVRSystem::GetHeadYaw() const
	{
		// Forward = -Z column of the head rotation; same convention as cCamera's yaw
		return atan2(m_mtxHead.m[0][2], m_mtxHead.m[2][2]);
	}

	float cVRSystem::GetHeadPitch() const
	{
		return asin(cMath::Clamp(-m_mtxHead.m[1][2], -1.0f, 1.0f));
	}

	cVector3f cVRSystem::GetPlayerHeadOffset() const
	{
		cVector3f vHead = m_mtxHead.GetTranslation();
		return cVector3f(vHead.x - mvHeadAnchor.x, cMath::Min(vHead.y, mfHeadMaxUp) + mfHeadYAdd, vHead.z - mvHeadAnchor.z);
	}

	//-----------------------------------------------------------------------

	cVector2f cVRSystem::GetStraightAheadUV(eVREye aEye) const
	{
		const float *t = mvEyeTan[aEye];
		return cVector2f(-t[0] / (t[1]-t[0]), -t[2] / (t[3]-t[2]));
	}

	//-----------------------------------------------------------------------
}
