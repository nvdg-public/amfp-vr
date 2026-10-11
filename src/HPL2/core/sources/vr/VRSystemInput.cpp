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

// Controller input for cVRSystem (SteamVR Input): buttons, sticks, hand poses, haptics.

#include "vr/VRSystem.h"

#include "system/LowLevelSystem.h"
#include "system/Platform.h"
#include "system/String.h"
#include "math/Math.h"
#include "graphics/Graphics.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/VertexBuffer.h"
#include "graphics/Texture.h"
#include "graphics/Mesh.h"
#include "graphics/SubMesh.h"
#include "graphics/Material.h"
#include "graphics/MaterialType.h"
#include "resources/Resources.h"
#include "resources/MaterialManager.h"
#include "resources/AnimationManager.h"

#ifdef WIN32
#include <windows.h>
#endif

#ifdef HPL_VR
	#include <openvr.h>
#endif

namespace hpl {

#ifdef HPL_VR
	extern vr::IVRSystem *gpHmd;
	extern vr::TrackedDevicePose_t gvPoses[vr::k_unMaxTrackedDeviceCount];
#endif

	//-----------------------------------------------------------------------

	static const char* gvButtonActionNames[eVRButton_LastEnum] =
	{
		"/actions/main/in/interact",
		"/actions/main/in/grab",
		"/actions/main/in/jump",
		"/actions/main/in/crouch",
		"/actions/main/in/recenter",
		"/actions/main/in/run",
		"/actions/main/in/lantern",
		"/actions/main/in/journal",
		"/actions/main/in/menu",
		"/actions/main/in/rotate",
		"/actions/main/in/interact_left",
		NULL,	// TurnUp (from the turn stick)
		NULL,	// TurnDown
	};

	// Shown in the game's "press X" texts (cLuxHelpFuncs) for the default Index bindings
	static const char* gvButtonNames[eVRButton_LastEnum] =
	{
		"Right trigger",
		"Right grip",
		"Right B",
		"Right A",
		"Right stick click",
		"Left stick click",
		"Left trigger",
		"Left A",
		"Left B",
		"Left grip",
		"Left trigger",
		"Right stick up",
		"Right stick down",
	};

	const char* cVRSystem::GetButtonName(eVRButton aButton)
	{
		if(aButton < 0 || aButton >= eVRButton_LastEnum) return "";
		return gvButtonNames[aButton];
	}

	//-----------------------------------------------------------------------

	bool cVRSystem::UsesHandPointer() const
	{
		return mbInputActive && mbHandPointer && mvAimValid[mActiveHand];
	}

	//-----------------------------------------------------------------------

	// How long Interact/Grab read as released after the active hand changed (ms): lets the game take the focus from
	// the new hand before it sees the press
	static const unsigned long kActiveHandSwitchHoldMs = 60;

	void cVRSystem::SetActiveHand(eVRHand aHand)
	{
		if(aHand == mActiveHand) return;
		mActiveHand = aHand;
		mlActiveHandSwitchTime = cPlatform::GetApplicationTime();
	}

	bool cVRSystem::GetButton(eVRButton aButton) const
	{
		if(aButton == eVRButton_Interact || aButton == eVRButton_Grab)
		{
			if(cPlatform::GetApplicationTime() - mlActiveHandSwitchTime < kActiveHandSwitchHoldMs) return false;
			return aButton == eVRButton_Interact ? GetHandTrigger(mActiveHand) : GetHandGrip(mActiveHand);
		}
		return mvButtons[aButton];
	}

	bool cVRSystem::GetHandAimPose(eVRHand aHand, cMatrixf &a_mtxPose) const
	{
		if(mbInputActive==false || mvAimValid[aHand]==false) return false;
		a_mtxPose = m_mtxAim[aHand];
		return true;
	}

	bool cVRSystem::GetHandGripPose(eVRHand aHand, cMatrixf &a_mtxPose) const
	{
		if(mbInputActive==false || mvGripValid[aHand]==false) return false;
		a_mtxPose = m_mtxGrip[aHand];
		return true;
	}

	bool cVRSystem::GetFingerCurls(eVRHand aHand, float *apCurls) const
	{
		if(mbInputActive==false || mvFingerValid[aHand]==false) return false;
		for(int f=0; f<5; ++f) apCurls[f] = mvFingerCurl[aHand][f];
		return true;
	}

	//-----------------------------------------------------------------------

	void cVRSystem::InitInput()
	{
#ifdef HPL_VR
		mbInputActive = false;
		if(vr::VRInput()==NULL)
		{
			Warning(" VR: no SteamVR Input interface, controllers disabled\n");
			return;
		}

		// Action manifest next to the exe (SteamVR wants an absolute path)
		tString sManifest = "vr_actions.json";
#ifdef WIN32
		char sExe[MAX_PATH];
		if(GetModuleFileNameA(NULL, sExe, MAX_PATH) > 0)
			sManifest = cString::GetFilePath(sExe) + "vr_actions.json";
#endif
		vr::EVRInputError err = vr::VRInput()->SetActionManifestPath(sManifest.c_str());
		if(err != vr::VRInputError_None)
		{
			Warning(" VR: SetActionManifestPath('%s') failed (%d), controllers disabled\n", sManifest.c_str(), (int)err);
			return;
		}

		bool bOk = vr::VRInput()->GetActionSetHandle("/actions/main", &mlActionSet) == vr::VRInputError_None;
		for(int i=0; i<eVRButton_LastEnum; ++i)
		{
			if(gvButtonActionNames[i]) bOk = bOk && vr::VRInput()->GetActionHandle(gvButtonActionNames[i], &mvButtonAction[i]) == vr::VRInputError_None;
		}
		bOk = bOk && vr::VRInput()->GetActionHandle("/actions/main/in/move", &mlMoveAction) == vr::VRInputError_None;
		bOk = bOk && vr::VRInput()->GetActionHandle("/actions/main/in/turn", &mlTurnAction) == vr::VRInputError_None;
		bOk = bOk && vr::VRInput()->GetActionHandle("/actions/main/in/interact_pull", &mlInteractPullAction) == vr::VRInputError_None;
		const char *vSide[eVRHand_LastEnum] = { "left", "right" };
		for(int i=0; i<eVRHand_LastEnum; ++i)
		{
			bOk = bOk && vr::VRInput()->GetActionHandle(("/actions/main/in/aim_" + tString(vSide[i])).c_str(), &mvAimAction[i]) == vr::VRInputError_None;
			bOk = bOk && vr::VRInput()->GetActionHandle(("/actions/main/in/grip_" + tString(vSide[i])).c_str(), &mvGripAction[i]) == vr::VRInputError_None;
			bOk = bOk && vr::VRInput()->GetActionHandle(("/actions/main/out/haptic_" + tString(vSide[i])).c_str(), &mvHapticAction[i]) == vr::VRInputError_None;
		}
		if(bOk==false)
		{
			Warning(" VR: could not get the SteamVR Input actions, controllers disabled\n");
			return;
		}
		// Left trigger soft pull (optional, like interact_pull)
		if(vr::VRInput()->GetActionHandle("/actions/main/in/interact_left_pull", &mlInteractLeftPullAction) != vr::VRInputError_None)
			mlInteractLeftPullAction = 0;
		// Finger curls for the hand models (optional: the hands stay open without them)
		for(int i=0; i<eVRHand_LastEnum; ++i)
		{
			if(vr::VRInput()->GetActionHandle(("/actions/main/in/skeleton_" + tString(vSide[i])).c_str(), &mvSkeletonAction[i]) != vr::VRInputError_None)
				mvSkeletonAction[i] = 0;
		}

		mbInputActive = true;
		Log(" VR controllers: SteamVR Input from '%s', pointer hand %s, pointer %s, stick dead zone %.2f\n",
			sManifest.c_str(), mPointerHand==eVRHand_Right ? "right" : "left", mbHandPointer ? "hand" : "head", mfStickDeadZone);
#endif
	}

	//-----------------------------------------------------------------------

	static cVector2f ApplyDeadZone(float afX, float afY, float afDeadZone)
	{
		cVector2f v(afX, afY);
		float fLen = v.Length();
		if(fLen <= afDeadZone) return cVector2f(0,0);
		// Rescale so the stick goes smoothly from 0 at the dead zone edge to 1 at full push
		float fNewLen = cMath::Min((fLen - afDeadZone) / (1.0f - afDeadZone), 1.0f);
		return v * (fNewLen / fLen);
	}

	void cVRSystem::UpdateInput()
	{
#ifdef HPL_VR
		if(mbInputActive==false) return;

		vr::VRActiveActionSet_t activeSet = {};
		activeSet.ulActionSet = mlActionSet;
		activeSet.ulRestrictedToDevice = vr::k_ulInvalidInputValueHandle;
		activeSet.ulSecondaryActionSet = vr::k_ulInvalidActionSetHandle;
		vr::VRInput()->UpdateActionState(&activeSet, sizeof(activeSet), 1);

		////////////////////////////
		// Buttons
		for(int i=0; i<eVRButton_LastEnum; ++i)
		{
			if(gvButtonActionNames[i]==NULL) continue;
			vr::InputDigitalActionData_t data;
			mvButtons[i] = vr::VRInput()->GetDigitalActionData(mvButtonAction[i], &data, sizeof(data), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None &&
							data.bActive && data.bState;
		}

		////////////////////////////
		// Interact trigger: a soft pull counts too (Input TriggerThreshold), not only the hard click at the end.
		// Hysteresis: released again below 70% of the threshold.
		vr::InputAnalogActionData_t analog;
		if(vr::VRInput()->GetAnalogActionData(mlInteractPullAction, &analog, sizeof(analog), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && analog.bActive)
		{
			static bool bPullDown = false;
			bPullDown = analog.x > (bPullDown ? mfTriggerThreshold*0.7f : mfTriggerThreshold);
			mvButtons[eVRButton_Interact] = mvButtons[eVRButton_Interact] || bPullDown;
		}
		if(mlInteractLeftPullAction && vr::VRInput()->GetAnalogActionData(mlInteractLeftPullAction, &analog, sizeof(analog), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && analog.bActive)
		{
			static bool bLeftPullDown = false;
			bLeftPullDown = analog.x > (bLeftPullDown ? mfTriggerThreshold*0.7f : mfTriggerThreshold);
			mvButtons[eVRButton_InteractLeft] = mvButtons[eVRButton_InteractLeft] || bLeftPullDown;
		}

		////////////////////////////
		// Sticks
		mvMoveAxis = 0;
		if(vr::VRInput()->GetAnalogActionData(mlMoveAction, &analog, sizeof(analog), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && analog.bActive)
			mvMoveAxis = ApplyDeadZone(analog.x, analog.y, mfStickDeadZone);
		mvTurnAxis = 0;
		if(vr::VRInput()->GetAnalogActionData(mlTurnAction, &analog, sizeof(analog), vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && analog.bActive)
			mvTurnAxis = ApplyDeadZone(analog.x, analog.y, mfStickDeadZone);

		// Turn stick up/down act as buttons (scrolling), with hysteresis; only when pushed mostly vertically
		float fAbsY = cMath::Abs(mvTurnAxis.y);
		bool bVertical = fAbsY > cMath::Abs(mvTurnAxis.x);
		mvButtons[eVRButton_TurnUp] = bVertical && mvTurnAxis.y > (mvButtons[eVRButton_TurnUp] ? 0.4f : 0.7f);
		mvButtons[eVRButton_TurnDown] = bVertical && mvTurnAxis.y < -(mvButtons[eVRButton_TurnDown] ? 0.4f : 0.7f);

		////////////////////////////
		// Hand poses (predicted for the frame being rendered, seated space like the HMD pose)
		for(int i=0; i<eVRHand_LastEnum; ++i)
		{
			vr::InputPoseActionData_t pose;
			mvAimValid[i] = vr::VRInput()->GetPoseActionDataForNextFrame(mvAimAction[i], vr::TrackingUniverseSeated, &pose, sizeof(pose),
							vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && pose.bActive && pose.pose.bPoseIsValid;
			if(mvAimValid[i]) m_mtxAim[i] = cMatrixf(	pose.pose.mDeviceToAbsoluteTracking.m[0][0], pose.pose.mDeviceToAbsoluteTracking.m[0][1], pose.pose.mDeviceToAbsoluteTracking.m[0][2], pose.pose.mDeviceToAbsoluteTracking.m[0][3],
													pose.pose.mDeviceToAbsoluteTracking.m[1][0], pose.pose.mDeviceToAbsoluteTracking.m[1][1], pose.pose.mDeviceToAbsoluteTracking.m[1][2], pose.pose.mDeviceToAbsoluteTracking.m[1][3],
													pose.pose.mDeviceToAbsoluteTracking.m[2][0], pose.pose.mDeviceToAbsoluteTracking.m[2][1], pose.pose.mDeviceToAbsoluteTracking.m[2][2], pose.pose.mDeviceToAbsoluteTracking.m[2][3],
													0, 0, 0, 1);

			mvGripValid[i] = vr::VRInput()->GetPoseActionDataForNextFrame(mvGripAction[i], vr::TrackingUniverseSeated, &pose, sizeof(pose),
							vr::k_ulInvalidInputValueHandle) == vr::VRInputError_None && pose.bActive && pose.pose.bPoseIsValid;
			if(mvGripValid[i]) m_mtxGrip[i] = cMatrixf(	pose.pose.mDeviceToAbsoluteTracking.m[0][0], pose.pose.mDeviceToAbsoluteTracking.m[0][1], pose.pose.mDeviceToAbsoluteTracking.m[0][2], pose.pose.mDeviceToAbsoluteTracking.m[0][3],
														pose.pose.mDeviceToAbsoluteTracking.m[1][0], pose.pose.mDeviceToAbsoluteTracking.m[1][1], pose.pose.mDeviceToAbsoluteTracking.m[1][2], pose.pose.mDeviceToAbsoluteTracking.m[1][3],
														pose.pose.mDeviceToAbsoluteTracking.m[2][0], pose.pose.mDeviceToAbsoluteTracking.m[2][1], pose.pose.mDeviceToAbsoluteTracking.m[2][2], pose.pose.mDeviceToAbsoluteTracking.m[2][3],
														0, 0, 0, 1);

			// Finger curls (skeletal summary: Index finger tracking, Quest/Frame touch sensors). Controllers with only an
			// estimated skeleton (Vive wands) are left invalid so the game closes the hand from the grip/trigger instead.
			vr::VRSkeletalSummaryData_t summary;
			vr::EVRSkeletalTrackingLevel trackingLevel = vr::VRSkeletalTracking_Estimated;
			mvFingerValid[i] = mvSkeletonAction[i] != 0 &&
								vr::VRInput()->GetSkeletalTrackingLevel(mvSkeletonAction[i], &trackingLevel) == vr::VRInputError_None &&
								trackingLevel >= vr::VRSkeletalTracking_Partial &&
								vr::VRInput()->GetSkeletalSummaryData(mvSkeletonAction[i], vr::VRSummaryType_FromDevice, &summary) == vr::VRInputError_None;
			if(mvFingerValid[i])
			{
				for(int f=0; f<5; ++f) mvFingerCurl[i][f] = cMath::Clamp(summary.flFingerCurl[f], 0.0f, 1.0f);
			}
		}

		////////////////////////////
		// Controller devices (for their models): device index by hand role, device pose from WaitGetPoses
		for(int i=0; i<eVRHand_LastEnum; ++i)
		{
			vr::TrackedDeviceIndex_t idx = gpHmd->GetTrackedDeviceIndexForControllerRole(
											i==eVRHand_Left ? vr::TrackedControllerRole_LeftHand : vr::TrackedControllerRole_RightHand);
			mvModelPoseValid[i] = false;
			if(idx == vr::k_unTrackedDeviceIndexInvalid || idx >= vr::k_unMaxTrackedDeviceCount)
			{
				mvDeviceIndex[i] = -1;
				continue;
			}
			mvDeviceIndex[i] = (int)idx;
			const vr::TrackedDevicePose_t &pose = gvPoses[idx];
			mvModelPoseValid[i] = pose.bPoseIsValid;
			if(pose.bPoseIsValid)
			{
				const vr::HmdMatrix34_t &m = pose.mDeviceToAbsoluteTracking;
				m_mtxModelPose[i] = cMatrixf(m.m[0][0], m.m[0][1], m.m[0][2], m.m[0][3],
											m.m[1][0], m.m[1][1], m.m[1][2], m.m[1][3],
											m.m[2][0], m.m[2][1], m.m[2][2], m.m[2][3],
											0, 0, 0, 1);
			}
		}

		// Log once when the controllers show up
		static bool bLoggedHands = false;
		if(bLoggedHands==false && (mvAimValid[0] || mvAimValid[1]))
		{
			bLoggedHands = true;
			Log("VR: controller poses received (left %d, right %d)\n", mvAimValid[0] ? 1 : 0, mvAimValid[1] ? 1 : 0);
		}
#endif
	}

	//-----------------------------------------------------------------------

	bool cVRSystem::GetControllerModelPose(eVRHand aHand, cMatrixf &a_mtxPose) const
	{
		if(mbActive==false || mvModelPoseValid[aHand]==false) return false;
		a_mtxPose = m_mtxModelPose[aHand];
		return true;
	}

	//-----------------------------------------------------------------------

	cMesh* cVRSystem::GetControllerMesh(eVRHand aHand, cGraphics *apGraphics, cResources *apResources)
	{
#ifdef HPL_VR
		if(mvControllerMesh[aHand]) return mvControllerMesh[aHand];
		if(mbActive==false || mvModelFailed[aHand] || mvDeviceIndex[aHand] < 0 || vr::VRRenderModels()==NULL) return NULL;

		////////////////////////////
		// Model name of the device in that hand
		if(mvModelName[aHand] == "")
		{
			char sName[vr::k_unMaxPropertyStringSize];
			vr::ETrackedPropertyError propErr;
			gpHmd->GetStringTrackedDeviceProperty(mvDeviceIndex[aHand], vr::Prop_RenderModelName_String, sName, sizeof(sName), &propErr);
			if(propErr != vr::TrackedProp_Success || sName[0]==0) return NULL;
			mvModelName[aHand] = sName;
			Log("VR: loading controller model '%s' (%s hand)\n", sName, aHand==eVRHand_Left ? "left" : "right");
		}

		////////////////////////////
		// Mesh and texture (asynchronous: "loading" until ready)
		vr::RenderModel_t *pModel = (vr::RenderModel_t*)mvRenderModel[aHand];
		if(pModel==NULL)
		{
			vr::EVRRenderModelError err = vr::VRRenderModels()->LoadRenderModel_Async(mvModelName[aHand].c_str(), &pModel);
			if(err == vr::VRRenderModelError_Loading) return NULL;
			if(err != vr::VRRenderModelError_None || pModel==NULL)
			{
				Warning("VR: could not load controller model '%s' (%d)\n", mvModelName[aHand].c_str(), (int)err);
				mvModelFailed[aHand] = true;
				return NULL;
			}
			mvRenderModel[aHand] = pModel;
		}
		vr::RenderModel_TextureMap_t *pTexMap = NULL;
		if(pModel->diffuseTextureId >= 0)
		{
			vr::EVRRenderModelError err = vr::VRRenderModels()->LoadTexture_Async(pModel->diffuseTextureId, &pTexMap);
			if(err == vr::VRRenderModelError_Loading) return NULL;
			if(err != vr::VRRenderModelError_None) pTexMap = NULL;
		}

		////////////////////////////
		// Engine mesh: same conventions as OpenVR (metres, Y up, -Z forward)
		tString sName = "VRController_" + tString(aHand==eVRHand_Left ? "Left" : "Right");
		iLowLevelGraphics *pLowGfx = apGraphics->GetLowLevel();
		iVertexBuffer *pVtx = pLowGfx->CreateVertexBuffer(eVertexBufferType_Hardware, eVertexBufferDrawType_Tri, eVertexBufferUsageType_Static);
		pVtx->CreateElementArray(eVertexBufferElement_Position, eVertexBufferElementFormat_Float, 4);
		pVtx->CreateElementArray(eVertexBufferElement_Normal, eVertexBufferElementFormat_Float, 3);
		pVtx->CreateElementArray(eVertexBufferElement_Color0, eVertexBufferElementFormat_Float, 4);
		pVtx->CreateElementArray(eVertexBufferElement_Texture0, eVertexBufferElementFormat_Float, 3);
		for(unsigned int v=0; v<pModel->unVertexCount; ++v)
		{
			const vr::RenderModel_Vertex_t &vtx = pModel->rVertexData[v];
			pVtx->AddVertexColor(eVertexBufferElement_Color0, cColor(1,1,1,1));
			pVtx->AddVertexVec3f(eVertexBufferElement_Position, cVector3f(vtx.vPosition.v[0], vtx.vPosition.v[1], vtx.vPosition.v[2]));
			pVtx->AddVertexVec3f(eVertexBufferElement_Normal, cVector3f(vtx.vNormal.v[0], vtx.vNormal.v[1], vtx.vNormal.v[2]));
			pVtx->AddVertexVec3f(eVertexBufferElement_Texture0, cVector3f(vtx.rfTextureCoord[0], vtx.rfTextureCoord[1], 0));
		}
		for(unsigned int t=0; t<pModel->unTriangleCount*3; ++t) pVtx->AddIndex(pModel->rIndexData[t]);
		int lVertices = (int)pModel->unVertexCount, lTriangles = (int)pModel->unTriangleCount;

		vr::VRRenderModels()->FreeRenderModel(pModel);
		mvRenderModel[aHand] = NULL;

		if(pVtx->Compile(eVertexCompileFlag_CreateTangents)==false)
		{
			Warning("VR: could not build the controller mesh\n");
			hplDelete(pVtx);
			if(pTexMap) vr::VRRenderModels()->FreeTexture(pTexMap);
			mvModelFailed[aHand] = true;
			return NULL;
		}

		iMaterialType *pMatType = apGraphics->GetMaterialType("SolidDiffuse");
		cMaterial *pMat = hplNew( cMaterial, (sName, _W(""), apGraphics, apResources, pMatType) );
		if(pTexMap)
		{
			iTexture *pTex = pLowGfx->CreateTexture(sName, eTextureType_2D, eTextureUsage_Normal);
			pTex->SetUseMipMaps(true);
			pTex->CreateFromRawData(cVector3l(pTexMap->unWidth, pTexMap->unHeight, 1), ePixelFormat_RGBA, (unsigned char*)pTexMap->rubTextureMapData);
			pTex->SetWrapSTR(eTextureWrap_ClampToEdge);
			pMat->SetTexture(eMaterialTexture_Diffuse, pTex);
			vr::VRRenderModels()->FreeTexture(pTexMap);
		}
		cResourceVarsObject vars;
		if(pMatType) pMatType->LoadVariables(pMat, &vars);
		pMat->Compile();

		cMesh *pMesh = hplNew( cMesh, (sName, _W(""), apResources->GetMaterialManager(), apResources->GetAnimationManager()) );
		cSubMesh *pSubMesh = pMesh->CreateSubMesh("Main");
		pSubMesh->SetMaterial(pMat);
		pSubMesh->SetVertexBuffer(pVtx);
		pMesh->IncUserCount(); // kept for the whole session
		mvControllerMesh[aHand] = pMesh;

		Log("VR: controller model ready (%d vertices, %d triangles, texture %s)\n", lVertices, lTriangles, pMat->GetTexture(eMaterialTexture_Diffuse) ? "yes" : "no");
		return pMesh;
#else
		return NULL;
#endif
	}

	//-----------------------------------------------------------------------

	void cVRSystem::TriggerHaptic(eVRHand aHand, float afSeconds, float afFrequency, float afAmplitude)
	{
#ifdef HPL_VR
		if(mbInputActive==false) return;
		vr::VRInput()->TriggerHapticVibrationAction(mvHapticAction[aHand], 0, afSeconds, afFrequency, cMath::Clamp(afAmplitude, 0.0f, 1.0f),
													vr::k_ulInvalidInputValueHandle);
#endif
	}

	//-----------------------------------------------------------------------
}
