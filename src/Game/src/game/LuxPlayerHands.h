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

#ifndef LUX_PLAYER_HANDS_H
#define LUX_PLAYER_HANDS_H

//----------------------------------------------

#include "LuxBase.h"

//----------------------------------------------

class cLuxPlayer;
class iLuxHandObject;
class cLuxHandObjectLoader;

//----------------------------------------------

class cLuxPlayerHands;

class cLuxPlayerHandsLoader : public cEntityLoader_Object
{
public:
	cLuxPlayerHandsLoader(const tString& asName, cLuxPlayerHands *apPlayerHands);

	void BeforeLoad(cXmlElement *apRootElem, const cMatrixf &a_mtxTransform,cWorld *apWorld, cResourceVarsObject *apInstanceVars);
	void AfterLoad(cXmlElement *apRootElem, const cMatrixf &a_mtxTransform,cWorld *apWorld, cResourceVarsObject *apInstanceVars);

private:
	cLuxPlayerHands *mpPlayerHands;
};

//----------------------------------------------

enum eLuxHandsState
{
	eLuxHandsState_Disabled, 
	eLuxHandsState_Holster,
	eLuxHandsState_Draw,
	eLuxHandsState_Idle,
	eLuxHandsState_HandObject,
};

//----------------------------------------------

class cLuxPlayerHands : public iLuxPlayerHelper
{
friend class cLuxPlayerHands_SaveData;
friend class cLuxPlayerHandsLoader;
public:	
	cLuxPlayerHands(cLuxPlayer *apPlayer);
	~cLuxPlayerHands();
	
	///////////////////////////////
	// General
	void OnStart();
	void Update(float afTimeStep);
	void PostUpdate(float afTimeStep);
	void Reset();

	void OnMapEnter(cLuxMap *apMap);
	void OnMapLeave(cLuxMap *apMap);

	void CreateWorldEntities(cLuxMap *apMap);
	void DestroyWorldEntities(cLuxMap *apMap);

	void RenderSolid(cRendererCallbackFunctions* apFunctions);
	void RenderTrans(cRendererCallbackFunctions* apFunctions);

	///////////////////////////////
	// Actions
	void PlayAnim(const tString& asAnim, bool abLoop);
	bool AnimOver();
	bool CheckAnimationEvent(float afRelTime);

	void DoAction(eLuxPlayerAction aAction, bool abPressed);

	///////////////////////////////
	// Properties
	void SetActiveHandObject(const tString& asName);
	iLuxHandObject* GetHandObject(const tString& asName);

	void SetCurrentHandObject(iLuxHandObject *apObject);
	iLuxHandObject* GetCurrentHandObject(){ return mpCurrentHandObject;}
	/** VR: the object being put away (stays in the hand while its light fades out), else NULL */
	iLuxHandObject* GetVRHolsterObject(){ return mpVRHolsterObject;}

	void SetState(eLuxHandsState aState);
	eLuxHandsState GetState(){ return mHandState; }

	const tString& GetCurrentAnimation(){ return msCurrentAnim; }
	cMeshEntity *GetHandsEntity(){ return mpHandsEntity;}

    
public://Only used by hand object
	float mfHandObjectChargeCount;
	int mlHandObjectState;
	bool mbHandObjectAttackDown;
	bool mbHandObjectInteractDown;
	float mfHandObjectAlpha;

private:
	void ResetHandObjectVars();

	void CreateHandEntity(cLuxMap *apMap);
	void DestroyHandEntity(cLuxMap *apMap);
	void CreateAndAttachHandObject(cLuxMap *apMap, iLuxHandObject *apHandObject);
	void HideAllHandObjects();
	
	void UpdatePlayerHandsPos(float afTimeStep);
	// VR: the held object (lantern) follows the lantern hand's controller
	bool GetVRHandsMatrix(const cMatrixf& a_mtxSeatedToWorld, cMatrixf& a_mtxHands);
	static void VRPreRenderCallback(void *apUserData);
	// VR: the controllers themselves (SteamVR models), shown at the hands
	void UpdateVRControllers(const cMatrixf& a_mtxSeatedToWorld);
	void DestroyVRControllers(cLuxMap *apMap);
	cMeshEntity *mvVRControllerEntity[2];
	// VR: hand models (Godot XR Tools hands, run\vr_assets\hands) at the controllers, fingers posed from code
	struct cVRFingerBone
	{
		int mlBoneIdx;
		int mlFinger;			// 0 thumb .. 4 pinky
		int mlJoint;			// 0 metacarpal, 1 proximal, 2 intermediate, 3 distal
		cMatrixf m_mtxBind;		// local bind transform
		cVector3f mvCurlAxis;	// in the bone's local frame: rotating about it bends the finger toward the palm
	};
	void UpdateVRHands(const cMatrixf& a_mtxSeatedToWorld);
	void SetupVRHandBones(int alHand);
	void PoseVRHandFingers(int alHand);
	cMeshEntity *mvVRHandEntity[2];
	std::vector<cVRFingerBone> mvVRFingerBones[2];
	float mvVRFingerCurl[2][5];

	iLuxHandObject* LoadHandObject(const tString& asName);
	iLuxHandObject* CreateObjectFromType(const tString& asName, eLuxHandObjectType aType);
	eLuxHandObjectType ToHandObjectType(const tString& asType);

	cMeshEntity *mpHandsEntity;
	cMesh *mpHandsMesh;
	std::vector<cAnimation*> mvHandAnimations;

	iLuxHandObject *mpCurrentHandObject;
	iLuxHandObject *mpVRHolsterObject;
	std::vector<iLuxHandObject*> mvHandObjects;

	cLuxHandObjectLoader *mpHandObjectLoader;

	float mfPosAddMul;

	int mlMaxCamRotations;
	tVector3fList mlstCamRotations;

	eLuxHandsState mHandState;
	tString msCurrentAnim;
};

//----------------------------------------------



#endif // LUX_PLAYER_HANDS_H
