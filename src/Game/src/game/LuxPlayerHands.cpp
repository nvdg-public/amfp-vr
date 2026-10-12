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

#include "LuxPlayerHands.h"
#include "LuxVR.h"

#include "LuxPlayer.h"
#include "LuxMapHandler.h"
#include "LuxMap.h"

#include "LuxHandObject_Melee.h"
#include "LuxHandObject_LightSource.h"


/////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPlayerHands::cLuxPlayerHands(cLuxPlayer *apPlayer) : iLuxPlayerHelper(apPlayer, "LuxPlayerHands")
{
	////////////////////
	//Init data

	//Add special entity type for loading the hands.
	gpBase->mpEngine->GetResources()->AddEntityLoader( hplNew( cLuxPlayerHandsLoader, ("PlayerHands", this) ) );

	// VR: place the held lantern and the controllers with the poses of each rendered frame
	mvVRControllerEntity[0] = mvVRControllerEntity[1] = NULL;
	mvVRHandEntity[0] = mvVRHandEntity[1] = NULL;
	mpVRHolsterObject = NULL;
	for(int i=0; i<2; ++i) for(int f=0; f<5; ++f) mvVRFingerCurl[i][f] = 0;
	if(cLuxVRSettings::Get().mbActive) cVRSystem::Get()->SetPreRenderCallback(VRPreRenderCallback, this);
	
	//Add entity loader for hand objects
	mpHandObjectLoader = hplNew( cLuxHandObjectLoader, ("HandObject") );
	gpBase->mpEngine->GetResources()->AddEntityLoader(mpHandObjectLoader);

	mlMaxCamRotations = gpBase->mpGameCfg->GetInt("Player_General", "HandsRotationSmoothNum", 0);
	mfPosAddMul = gpBase->mpGameCfg->GetFloat("Player_General", "HandsPosAddMul", 0);

	mpHandsMesh = NULL;

}

cLuxPlayerHands::~cLuxPlayerHands()
{
	if(mpHandsMesh) 
	{
		gpBase->mpEngine->GetResources()->GetMeshManager()->Destroy(mpHandsMesh);
	}

	for(size_t i=0; i<mvHandAnimations.size(); ++i)
	{
		gpBase->mpEngine->GetResources()->GetAnimationManager()->Destroy(mvHandAnimations[i]);
	}
	
	STLDeleteAll(mvHandObjects);

	//Resources delete hand object loader by itself!
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// ENTITY LOADER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPlayerHandsLoader::cLuxPlayerHandsLoader(const tString& asName, cLuxPlayerHands *apPlayerHands) : cEntityLoader_Object(asName)
{
	mpPlayerHands = apPlayerHands;
}

//-----------------------------------------------------------------------

void cLuxPlayerHandsLoader::BeforeLoad(cXmlElement *apRootElem, const cMatrixf &a_mtxTransform,cWorld *apWorld, cResourceVarsObject *apInstanceVars)
{
	
}

//-----------------------------------------------------------------------

void cLuxPlayerHandsLoader::AfterLoad(cXmlElement *apRootElem, const cMatrixf &a_mtxTransform,cWorld *apWorld, cResourceVarsObject *apInstanceVars)
{
	mpPlayerHands->mpHandsEntity = mpEntity;
	if(mpEntity) mpEntity->SetRenderFlagBit(eRenderableFlag_ShadowCaster,false);
	if(mpEntity) mpEntity->SetUpdateBonesWhenCulled(true);
	
	if(mpMesh && mpPlayerHands->mpHandsMesh==NULL)
	{
		mpMesh->IncUserCount();
		mpPlayerHands->mpHandsMesh = mpMesh;
	}

	if(mpPlayerHands->mvHandAnimations.empty() && mpEntity)
	{
		for(int i=0; i<mpEntity->GetAnimationStateNum(); ++i)
		{
			cAnimationState *pAnimState = mpEntity->GetAnimationState(i);
            cAnimation *pAnim = pAnimState->GetAnimation();
			pAnim->IncUserCount();
			mpPlayerHands->mvHandAnimations.push_back(pAnim);
		}
	}
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxPlayerHands::OnStart()
{
	
}

void cLuxPlayerHands::Reset()
{
	////////////////////
	//Init variables
	mpCurrentHandObject = NULL;
	mpHandsEntity = NULL;

	mHandState = eLuxHandsState_Disabled;
	mpVRHolsterObject = NULL;
	// VR: the controller/hand models died with the old world (Reset runs instead of OnMapLeave when you quit to the menu)
	mvVRControllerEntity[0] = mvVRControllerEntity[1] = NULL;
	mvVRHandEntity[0] = mvVRHandEntity[1] = NULL;
	mfHandObjectAlpha =0;
 
	msCurrentAnim = "";

	mlstCamRotations.clear();

	for(size_t i=0; i<mvHandObjects.size(); ++i)
	{
		iLuxHandObject *pHandObject = mvHandObjects[i];
		pHandObject->Reset();
	}

	////////////////////
	//Init hand object variables
	ResetHandObjectVars();
}

//-----------------------------------------------------------------------

void cLuxPlayerHands::Update(float afTimeStep)
{
	for(int i=0; i<mpHandsEntity->GetAnimationStateNum(); ++i)
	{
		cAnimationState *pAnim = mpHandsEntity->GetAnimationState(i);
	}
	
	////////////////////////////
	// Disabled
	if(mHandState == eLuxHandsState_Disabled)
	{
		return;
	}
	////////////////////////////
	// Holster
	else if(mHandState == eLuxHandsState_Holster)
	{
		// VR: no holster animation; done when the put-away object has faded out
		bool bHolstered = mpVRHolsterObject ? mfHandObjectAlpha <= 0.0f : AnimOver();
		if(bHolstered)
		{
			if(mpVRHolsterObject)
			{
				mpVRHolsterObject = NULL;
				HideAllHandObjects();
			}
			////////////////////
			//Draw up new object
			if(mpCurrentHandObject)
			{
				cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();

				CreateAndAttachHandObject(pMap, mpCurrentHandObject);
				PlayAnim(mpCurrentHandObject->GetAnimDraw(),false);
				
				mHandState = eLuxHandsState_Draw;
			}
			////////////////////
			//Disable hands
			else
			{
				HideAllHandObjects();
				mpHandsEntity->SetVisible(false);
				mHandState = eLuxHandsState_Disabled;
			}
		}
	}	
	////////////////////////////
	// Draw
	else if(mHandState == eLuxHandsState_Draw)
	{
		if(AnimOver())
		{
			PlayAnim(mpCurrentHandObject->GetAnimIdle(),true);
			mHandState = eLuxHandsState_Idle;
		}

	}		
	////////////////////////////
	// Idle
	else if(mHandState == eLuxHandsState_Idle)
	{

	}
	////////////////////////////
	// Hand Object Specific
	else if(mHandState == eLuxHandsState_HandObject)
	{
		if(mpCurrentHandObject)
		{
			//////////////////////////////
			//Animation is over check
			if(AnimOver())
			{
				if(mpCurrentHandObject->AnimationIsOver())
				{
					PlayAnim(mpCurrentHandObject->GetAnimIdle(),true);
					mHandState = eLuxHandsState_Idle;
					return;
				}
			}
		}
	}

	////////////////////
	// Update Hand Object (need to do like this, since current might not be drawn yet!)
	for(size_t i=0; i<mvHandObjects.size(); ++i)
	{
		iLuxHandObject *pHandObject = mvHandObjects[i];
		if(pHandObject->GetMeshEntity() && pHandObject->GetMeshEntity()->IsActive())
		{
			pHandObject->Update(afTimeStep);
		}
	}
}


//-----------------------------------------------------------------------

void cLuxPlayerHands::PostUpdate(float afTimeStep)
{
	UpdatePlayerHandsPos(afTimeStep);

	// VR: hide the arm (the hands model's own submeshes), keep the held object (lantern) visible. The hands
	// entity stays visible/animated so the attached object keeps following its bone.
	if(mpHandsEntity && cLuxVRSettings::Get().mbActive && cLuxVRSettings::Get().mbShowArm==false && mpHandsEntity->IsVisible())
	{
		for(int i=0; i<mpHandsEntity->GetSubMeshEntityNum(); ++i)
		{
			cSubMeshEntity *pSub = mpHandsEntity->GetSubMeshEntity(i);
			if(pSub->GetVisibleVar()) pSub->SetVisible(false);
		}
	}
}

//-----------------------------------------------------------------------

void cLuxPlayerHands::OnMapEnter(cLuxMap *apMap)
{
	//////////////////////////
	// If a hand object is loaded, create it.
	if(mpCurrentHandObject)
	{
		CreateAndAttachHandObject(apMap, mpCurrentHandObject);
		
		mpHandsEntity->SetVisible(true);
		mpHandsEntity->PlayName(mpCurrentHandObject->GetAnimIdle(),true, true);
	}

	mlstCamRotations.clear();
}

//-----------------------------------------------------------------------

void cLuxPlayerHands::OnMapLeave(cLuxMap *apMap)
{
	mpHandsEntity = NULL;
	mpVRHolsterObject = NULL;
	mvVRControllerEntity[0] = mvVRControllerEntity[1] = NULL;
	mvVRHandEntity[0] = mvVRHandEntity[1] = NULL;
	for(size_t i=0; i<mvHandObjects.size(); ++i)
	{
		iLuxHandObject *pHandObject = mvHandObjects[i];
		pHandObject->ResetEntityContainers();
	}
}

//-----------------------------------------------------------------------

void cLuxPlayerHands::CreateWorldEntities(cLuxMap *apMap)
{
	//////////////////////////
	// Create the hand entity
	CreateHandEntity(apMap);
}

void cLuxPlayerHands::DestroyWorldEntities(cLuxMap *apMap)
{
	/////////////////////////
	// Destroy the hand entity
	DestroyHandEntity(apMap);
	DestroyVRControllers(apMap);

	///////////////////////////////////
	// Destroy all hand object entities
	for(size_t i=0; i<mvHandObjects.size(); ++i)
	{
		iLuxHandObject *pHandObject = mvHandObjects[i];
		pHandObject->DestroyEntity(apMap);
	}
}

//-----------------------------------------------------------------------

void cLuxPlayerHands::RenderSolid(cRendererCallbackFunctions* apFunctions)
{
	if(mpCurrentHandObject) mpCurrentHandObject->RenderSolid(apFunctions);
}

void cLuxPlayerHands::RenderTrans(cRendererCallbackFunctions* apFunctions)
{
	if(mpCurrentHandObject) mpCurrentHandObject->RenderTrans(apFunctions);
}

//-----------------------------------------------------------------------

void cLuxPlayerHands::PlayAnim(const tString& asAnim, bool abLoop)
{
	mpHandsEntity->PlayFadeToName(asAnim, abLoop, 0.2f);
	msCurrentAnim = asAnim;
}

bool cLuxPlayerHands::AnimOver()
{
	return mpHandsEntity->AnimationIsOver(msCurrentAnim);
}

//-----------------------------------------------------------------------

bool cLuxPlayerHands::CheckAnimationEvent(float afRelTime)
{
	cAnimationState *pAnim = mpHandsEntity->GetAnimationStateFromName(msCurrentAnim);
	if(pAnim == NULL) return false;

	float fPrevPos = pAnim->GetPreviousTimePosition() / pAnim->GetLength();
	float fPos = pAnim->GetRelativeTimePosition();
    
	return afRelTime > fPrevPos && afRelTime <= fPos;
}

//-----------------------------------------------------------------------

void cLuxPlayerHands::DoAction(eLuxPlayerAction aAction, bool abPressed)
{
	if(mpCurrentHandObject==NULL) return;
	if(mHandState != eLuxHandsState_Idle && mHandState != eLuxHandsState_HandObject) return;

	if(mpCurrentHandObject->DoAction(aAction, abPressed))
	{
		mHandState = eLuxHandsState_HandObject;
	}
}

//-----------------------------------------------------------------------

void cLuxPlayerHands::SetActiveHandObject(const tString& asName)
{
	if(mpHandsEntity==NULL) return; //Make sure the 

	if(asName != "")
	{
		iLuxHandObject* pObject = GetHandObject(asName);
		if(pObject) SetCurrentHandObject(pObject);
	}
	else
	{
		SetCurrentHandObject(NULL);
	}
}

//-----------------------------------------------------------------------

iLuxHandObject* cLuxPlayerHands::GetHandObject(const tString& asName)
{
	//////////////////////////////////////
	// Search in already loaded objects
	for(size_t i=0; i<mvHandObjects.size(); ++i)
	{
		iLuxHandObject *apObject = mvHandObjects[i];
        if(asName == apObject->GetName()) return apObject;
	}

	//////////////////////////////////////
	// If not found try loading
	return LoadHandObject(asName);
}


//-----------------------------------------------------------------------

void cLuxPlayerHands::SetCurrentHandObject(iLuxHandObject *apObject)
{
	if(mpCurrentHandObject == apObject) return;

	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();

	ResetHandObjectVars();
	if(apObject) mpVRHolsterObject = NULL; // taken out again while fading: draw as usual
	
	////////////////////////
	// VR with the lantern in the hand: no holster animation (it would play at the flat game's camera spot);
	// the old object stays in the hand while its light fades out (Update's Holster state), then disappears
	const cLuxVRSettings &vrSettings = cLuxVRSettings::Get();
	if(mpCurrentHandObject && vrSettings.mbActive && vrSettings.mbLanternInHand)
	{
		mpVRHolsterObject = mpCurrentHandObject;
		mHandState = eLuxHandsState_Holster;
	}
	////////////////////////
	// Previous hand set
	else if(mpCurrentHandObject)
	{
		PlayAnim(mpCurrentHandObject->GetAnimHolster(),false);
		
		mHandState = eLuxHandsState_Holster;
	}
	////////////////////////
	// Draw directly
	else if(apObject)
	{
		CreateAndAttachHandObject(pMap, apObject);
		PlayAnim(apObject->GetAnimDraw(),false);
		mpHandsEntity->SetVisible(true);

		mHandState = eLuxHandsState_Draw;
	}
	mpCurrentHandObject = apObject;
}


//-----------------------------------------------------------------------

void cLuxPlayerHands::SetState(eLuxHandsState aState)
{ 
	mHandState = aState; 
}


//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxPlayerHands::ResetHandObjectVars()
{
	mfHandObjectChargeCount =0;
	mlHandObjectState =0;
	mbHandObjectAttackDown = false;
	mbHandObjectInteractDown = false;

	//NOTE: Do note reset the alpha here!
}

//-----------------------------------------------------------------------

void cLuxPlayerHands::CreateHandEntity(cLuxMap *apMap)
{
	if(mpHandsEntity) return;

	apMap->GetWorld()->CreateEntity("PlayerHands", cMatrixf::Identity, "models/player/hands/hands.ent");
	mpHandsEntity->SetVisible(false);
	mpHandsEntity->Stop();
	mpHandsEntity->SetRenderFlagBit(eRenderableFlag_ShadowCaster,false);
	mpHandsEntity->SetRenderFlagBit(eRenderableFlag_VisibleInReflection,false);
}

void cLuxPlayerHands::DestroyHandEntity(cLuxMap *apMap)
{
	if(mpHandsEntity)
	{
		apMap->GetWorld()->DestroyMeshEntity(mpHandsEntity);
		mpHandsEntity = NULL;
	}
}

//-----------------------------------------------------------------------

void cLuxPlayerHands::CreateAndAttachHandObject(cLuxMap *apMap, iLuxHandObject *apHandObject)
{
	HideAllHandObjects();

	///////////////////////
	//Load object
	mpHandObjectLoader->SetHandObject(apHandObject);
	apHandObject->CreateEntity(apMap);

	///////////////////////
	//Attach object
	cMeshEntity *pMeshEntity = apHandObject->GetMeshEntity();
	if(pMeshEntity)
	{
		cBoneState *pBone = mpHandsEntity->GetBoneStateFromName(apHandObject->GetAttachBoneName());
		if(pBone==NULL)
		{
			Error("Could not find bone '%s' in player hands model!\n", apHandObject->GetAttachBoneName().c_str());
			return;
		}
				
		pBone->AddEntity(pMeshEntity);
	}
	apHandObject->SetSetEntitiesVisible(true);

	mfHandObjectAlpha =0;
}

//-----------------------------------------------------------------------

void cLuxPlayerHands::HideAllHandObjects()
{
	for(size_t i=0; i<mvHandObjects.size(); ++i)
	{
		iLuxHandObject *pHandObject = mvHandObjects[i];
		pHandObject->SetSetEntitiesVisible(false);
	}
}

//-----------------------------------------------------------------------

/**
 * VR: hands model matrix that puts the current hand object (lantern) in the lantern hand: at the controller's grip,
 * hanging upright (only the hand's yaw), plus the Lantern Offset from vr_settings.cfg. The arm is hidden in VR, so
 * only the object is seen. False if not in VR, no object is held or the controller is not tracked.
 */
bool cLuxPlayerHands::GetVRHandsMatrix(const cMatrixf& a_mtxSeatedToWorld, cMatrixf& a_mtxHands)
{
	const cLuxVRSettings &settings = cLuxVRSettings::Get();
	if(settings.mbActive==false || settings.mbLanternInHand==false) return false;
	iLuxHandObject *pHandObject = mpCurrentHandObject ? mpCurrentHandObject : mpVRHolsterObject;
	if(mpHandsEntity==NULL || pHandObject==NULL) return false;
	cMeshEntity *pObject = pHandObject->GetMeshEntity();
	if(pObject==NULL) return false;

	cVRSystem *pVR = cVRSystem::Get();
	cMatrixf mtxGrip;
	if(pVR->GetHandGripPose(pVR->GetLanternHand(), mtxGrip)==false) return false;

	// Hand in seated space: grip position, rotation = yaw of the hand's forward (-Z) direction
	cVector3f vFwd(-mtxGrip.m[0][2], 0, -mtxGrip.m[2][2]);
	float fYaw = vFwd.SqrLength() > 1e-6f ? atan2(-vFwd.x, -vFwd.z) : 0;
	// Hands LanternTurn: twisting the hand never turns the lantern in Fixed and Arm modes.
	//  Fixed: always the body's forward (seated space -Z; stick turning still turns it).
	//  Arm:   points from the head toward the hand (horizontal), so moving the hand left/right sweeps the beam.
	//  Point: the way the controller points (its aim pose), so turning the wrist left/right aims the beam, while
	//         rolling/twisting the hand around that pointing direction does not.
	cMatrixf mtxAim;
	if(settings.mLanternTurn == cLuxVRSettings::eLanternTurn_Point && pVR->GetHandAimPose(pVR->GetLanternHand(), mtxAim))
	{
		cVector3f vAimFwd(-mtxAim.m[0][2], 0, -mtxAim.m[2][2]);
		if(vAimFwd.SqrLength() > 1e-6f) fYaw = atan2(-vAimFwd.x, -vAimFwd.z);
	}
	else if(settings.mLanternTurn == cLuxVRSettings::eLanternTurn_Fixed)
	{
		fYaw = 0;
	}
	else if(settings.mLanternTurn == cLuxVRSettings::eLanternTurn_Arm)
	{
		// Measured from a pivot behind the head (Hands LanternArmPivot): with the arm resting the hand is close
		// to the head, which would make small hand moves swing the beam wildly
		cVector3f vToHand = mtxGrip.GetTranslation() - (pVR->GetHeadPosition() + cVector3f(0, 0, settings.mfLanternArmPivot));
		vToHand.y = 0;
		float fLen = vToHand.Length();
		// Hand (almost) under the head: fall back smoothly to the body's forward
		float fW = cMath::Clamp((fLen - 0.05f) / 0.1f, 0.0f, 1.0f);
		cVector3f vDir = cVector3f(0,0,-1) * (1.0f - fW) + (fLen > 1e-4f ? vToHand / fLen : cVector3f(0,0,-1)) * fW;
		fYaw = vDir.SqrLength() > 1e-6f ? atan2(-vDir.x, -vDir.z) : 0;
	}
	cMatrixf mtxHand = settings.mbLanternUpright ? cMath::MatrixRotateY(fYaw) : mtxGrip.GetRotation();
	mtxHand.SetTranslation(mtxGrip.GetTranslation() + cMath::MatrixMul(mtxHand.GetRotation(), settings.mvLanternOffset));
	// Hands LanternYaw: turn the lantern itself around its vertical (counterclockwise seen from above)
	cMatrixf mtxYaw = cMath::MatrixRotateY(settings.mfLanternYaw);
	mtxHand = cMath::MatrixMul(mtxHand, mtxYaw);
	cMatrixf mtxHandWorld = cMath::MatrixMul(a_mtxSeatedToWorld, mtxHand);

	// Object relative to the hands model (follows the hands animation's attach bone); cancel it out
	cMatrixf mtxObjectRel = cMath::MatrixMul(cMath::MatrixInverse(mpHandsEntity->GetWorldMatrix()), pObject->GetWorldMatrix());
	a_mtxHands = cMath::MatrixMul(mtxHandWorld, cMath::MatrixInverse(mtxObjectRel));
	return true;
}

void cLuxPlayerHands::VRPreRenderCallback(void *apUserData)
{
	cLuxPlayerHands *pHands = static_cast<cLuxPlayerHands*>(apUserData);
	pHands->UpdateVRControllers(cVRSystem::Get()->GetRenderSeatedToWorld());
	if(pHands->mpHandsEntity==NULL || pHands->mpHandsEntity->IsVisible()==false) return;
	cMatrixf mtxHands;
	if(pHands->GetVRHandsMatrix(cVRSystem::Get()->GetRenderSeatedToWorld(), mtxHands))
		pHands->mpHandsEntity->SetMatrix(mtxHands);
}

//-----------------------------------------------------------------------

/**
 * VR: shows the controllers (SteamVR's own models, e.g. the Index controllers) where the real ones are. The models
 * load in the background; an entity is created in the current map once a model is ready.
 */
void cLuxPlayerHands::UpdateVRControllers(const cMatrixf& a_mtxSeatedToWorld)
{
	const cLuxVRSettings &settings = cLuxVRSettings::Get();
	if(settings.mbActive==false) return;
	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	if(pMap==NULL || mpHandsEntity==NULL) return; // only in a map with the player set up

	cVRSystem *pVR = cVRSystem::Get();
	for(int i=0; i<2; ++i)
	{
		eVRHand hand = (eVRHand)i;
		if(mvVRControllerEntity[i]==NULL)
		{
			if(settings.mbShowControllers==false) continue;
			cMesh *pMesh = pVR->GetControllerMesh(hand, gpBase->mpEngine->GetGraphics(), gpBase->mpEngine->GetResources());
			if(pMesh==NULL) continue;
			pMesh->IncUserCount(); // released by the entity when it is destroyed
			cMeshEntity *pEntity = pMap->GetWorld()->CreateMeshEntity(i==0 ? "VRControllerLeft" : "VRControllerRight", pMesh, false);
			pEntity->SetRenderFlagBit(eRenderableFlag_ShadowCaster, false);
			pEntity->SetRenderFlagBit(eRenderableFlag_VisibleInReflection, false);
			mvVRControllerEntity[i] = pEntity;
		}

		cMatrixf mtxPose;
		bool bVisible = settings.mbShowControllers && pVR->GetControllerModelPose(hand, mtxPose);
		if(bVisible) mvVRControllerEntity[i]->SetMatrix(cMath::MatrixMul(a_mtxSeatedToWorld, mtxPose));
		if(mvVRControllerEntity[i]->IsVisible() != bVisible) mvVRControllerEntity[i]->SetVisible(bVisible);
	}

	UpdateVRHands(a_mtxSeatedToWorld);
}

void cLuxPlayerHands::DestroyVRControllers(cLuxMap *apMap)
{
	for(int i=0; i<2; ++i)
	{
		if(mvVRControllerEntity[i]) apMap->GetWorld()->DestroyMeshEntity(mvVRControllerEntity[i]);
		mvVRControllerEntity[i] = NULL;
		if(mvVRHandEntity[i]) apMap->GetWorld()->DestroyMeshEntity(mvVRHandEntity[i]);
		mvVRHandEntity[i] = NULL;
	}
}

//-----------------------------------------------------------------------

/**
 * VR: hand models (Godot XR Tools hands, CC0, converted by src\tools\hands) placed at the controllers' pointing
 * pose. They are skinned meshes without animations: the fingers are bent here, bone by bone, from SteamVR's finger
 * curls (Index finger tracking). The hand holding the lantern closes around its handle.
 */
void cLuxPlayerHands::UpdateVRHands(const cMatrixf& a_mtxSeatedToWorld)
{
	const cLuxVRSettings &settings = cLuxVRSettings::Get();
	if(settings.mbShowHands==false) return;
	cLuxMap *pMap = gpBase->mpMapHandler->GetCurrentMap();
	if(pMap==NULL || mpHandsEntity==NULL) return;

	// The hand files live next to the exe (run\vr_assets), not in the game folder
	static bool bAddedDir = false;
	static bool bLoadFailed = false;
	if(bAddedDir==false)
	{
		bAddedDir = true;
		gpBase->mpEngine->GetResources()->AddResourceDir(cVRSystem::GetAssetDir(), true);
	}
	if(bLoadFailed) return;

	cVRSystem *pVR = cVRSystem::Get();
	bool bLanternOut = settings.mbLanternInHand && (mpCurrentHandObject || mpVRHolsterObject) && mpHandsEntity->IsVisible();
	for(int i=0; i<2; ++i)
	{
		eVRHand hand = (eVRHand)i;
		if(mvVRHandEntity[i]==NULL)
		{
			const char *sMesh = i==eVRHand_Left ? "vr_hand_l.msh" : "vr_hand_r.msh";
			cMesh *pMesh = gpBase->mpEngine->GetResources()->GetMeshManager()->CreateMesh(sMesh); // released by the entity
			if(pMesh==NULL || pMesh->GetSkeleton()==NULL)
			{
				Error("VR: could not load hand model '%s' (run\\vr_assets\\hands); showing no hands\n", sMesh);
				bLoadFailed = true;
				return;
			}
			cMeshEntity *pEntity = pMap->GetWorld()->CreateMeshEntity(i==eVRHand_Left ? "VRHandLeft" : "VRHandRight", pMesh, false);
			pEntity->SetRenderFlagBit(eRenderableFlag_ShadowCaster, false);
			pEntity->SetRenderFlagBit(eRenderableFlag_VisibleInReflection, false);
			mvVRHandEntity[i] = pEntity;
			SetupVRHandBones(i);
			Log("VR: hand model '%s' loaded (%d bones, %d finger bones posed)\n", sMesh, pMesh->GetSkeleton()->GetBoneNum(), (int)mvVRFingerBones[i].size());
		}

		// Placement: the right hand's settings, mirrored (x -> -x) for the left hand
		cMatrixf mtxAim;
		bool bVisible = pVR->GetHandAimPose(hand, mtxAim);
		// Debug: HPL_VR_FAKE_HANDS=1 shows the hands (tracked or not) in front of the (seated) head, fingers opening and closing
		static const char *pFake = getenv("HPL_VR_FAKE_HANDS");
		bool bFake = pFake && pFake[0]=='1';
		if(bFake)
		{
			mtxAim = cMath::MatrixRotateX(cMath::ToRad(-20.0f));
			mtxAim.SetTranslation(cVector3f(i==eVRHand_Left ? -0.15f : 0.15f, -0.25f, -0.35f));
			bVisible = true;
		}
		if(bVisible)
		{
			float fMirror = i==eVRHand_Left ? -1.0f : 1.0f;
			const cVector3f &vRot = settings.mvHandRotation;
			cMatrixf mtxLocal = cMath::MatrixMul(cMath::MatrixRotateX(vRot.x),
								cMath::MatrixMul(cMath::MatrixRotateY(vRot.y * fMirror), cMath::MatrixRotateZ(vRot.z * fMirror)));
			mtxLocal.SetTranslation(cVector3f(settings.mvHandOffset.x * fMirror, settings.mvHandOffset.y, settings.mvHandOffset.z));
			mvVRHandEntity[i]->SetMatrix(cMath::MatrixMul(a_mtxSeatedToWorld, cMath::MatrixMul(mtxAim, mtxLocal)));

			// Finger curls: SteamVR skeletal summary; without it (Vive wands) the hand's grip/trigger close it
			float vTarget[5] = {0,0,0,0,0};
			if(bFake)
			{
				float fT = 0.5f + 0.5f*sin((float)cPlatform::GetApplicationTime() * 0.0015f);
				for(int f=0; f<5; ++f) vTarget[f] = fT;
			}
			else if(pVR->GetFingerCurls(hand, vTarget)==false)
			{
				float fGrip = pVR->GetHandGrip(hand) ? 1.0f : 0.0f;
				vTarget[0] = fGrip * 0.7f; vTarget[2] = vTarget[3] = vTarget[4] = fGrip;
				vTarget[1] = pVR->GetHandTrigger(hand) ? 1.0f : fGrip;
			}
			// The lantern hand holds the handle
			if(bLanternOut && hand==pVR->GetLanternHand())
			{
				vTarget[0] = cMath::Max(vTarget[0], 0.6f);
				for(int f=1; f<5; ++f) vTarget[f] = cMath::Max(vTarget[f], 0.85f);
			}
			// Light smoothing (this runs per logic step and per rendered frame)
			for(int f=0; f<5; ++f) mvVRFingerCurl[i][f] += (vTarget[f] - mvVRFingerCurl[i][f]) * 0.5f;
			PoseVRHandFingers(i);
			if(bFake)
			{
				static int lFakeLogs = 0;
				if(lFakeLogs++ < 2)
					Log("VR fake hand %d at %s, camera at %s\n", i, mvVRHandEntity[i]->GetWorldPosition().ToString().c_str(),
						mpPlayer->GetCamera()->GetPosition().ToString().c_str());
			}
		}
		if(mvVRHandEntity[i]->IsVisible() != bVisible) mvVRHandEntity[i]->SetVisible(bVisible);
	}
}

/**
 * Finds the finger bones of a hand model by name (OpenXR-style: Index_Proximal_R, ...) and, for each, the axis in
 * its own frame that bends it toward the palm: perpendicular to the bone and the palm's normal. The models hold
 * the hand as around a controller: fingers along -Z, thumb up (+Y), palm facing -X (right hand) or +X (left hand).
 */
void cLuxPlayerHands::SetupVRHandBones(int alHand)
{
	mvVRFingerBones[alHand].clear();
	cSkeleton *pSkeleton = mvVRHandEntity[alHand]->GetMesh()->GetSkeleton();
	const char *vFinger[5] = { "Thumb_", "Index_", "Middle_", "Ring_", "Little_" };
	const char *vJoint[4] = { "Metacarpal", "Proximal", "Intermediate", "Distal" };
	for(int b=0; b<pSkeleton->GetBoneNum(); ++b)
	{
		cBone *pBone = pSkeleton->GetBoneByIndex(b);
		const tString &sName = pBone->GetName();
		int lFinger = -1, lJoint = -1;
		for(int f=0; f<5 && lFinger<0; ++f) if(sName.find(vFinger[f])==0) lFinger = f;
		for(int j=0; j<4 && lJoint<0; ++j) if(sName.find(vJoint[j])!=tString::npos) lJoint = j;
		if(lFinger<0 || lJoint<0 || pBone->GetChildList()->empty()) continue;

		// Bone direction toward its child joint, in model (bind) space
		cBone *pChild = pBone->GetChildList()->front();
		cVector3f vDir = pChild->GetWorldTransform().GetTranslation() - pBone->GetWorldTransform().GetTranslation();
		if(vDir.SqrLength() < 1e-8f) continue;
		vDir.Normalize();
		cVector3f vPalmNormal(alHand==eVRHand_Left ? 1.0f : -1.0f, 0, 0);
		// The thumb sits on top of the controller: it bends down onto the buttons (and a little toward the palm)
		cVector3f vBendTo = lFinger==0 ? cMath::Vector3Normalize(vPalmNormal*0.4f + cVector3f(0,-1,0)) : vPalmNormal;
		cVector3f vAxisModel = cMath::Vector3Cross(vDir, vBendTo);
		if(vAxisModel.SqrLength() < 1e-8f) continue;
		vAxisModel.Normalize();

		cVRFingerBone fb;
		fb.mlBoneIdx = b;
		fb.mlFinger = lFinger;
		fb.mlJoint = lJoint;
		fb.m_mtxBind = pBone->GetLocalTransform();
		fb.mvCurlAxis = cMath::MatrixMul(cMath::MatrixInverse(pBone->GetWorldTransform()).GetRotation(), vAxisModel);
		fb.mvCurlAxis.Normalize();
		mvVRFingerBones[alHand].push_back(fb);
	}
}

void cLuxPlayerHands::PoseVRHandFingers(int alHand)
{
	// Share of the full bend per joint (metacarpal, proximal, intermediate, distal); the thumb has no intermediate
	static const float vFingerJoint[4] = { 0.1f, 1.0f, 1.1f, 0.8f };
	static const float vThumbJoint[4] = { 0.35f, 0.55f, 0.0f, 0.6f };
	float fFull = cMath::ToRad(cLuxVRSettings::Get().mfFingerCurl);
	cMeshEntity *pEntity = mvVRHandEntity[alHand];
	for(size_t i=0; i<mvVRFingerBones[alHand].size(); ++i)
	{
		const cVRFingerBone &fb = mvVRFingerBones[alHand][i];
		float fShare = fb.mlFinger==0 ? vThumbJoint[fb.mlJoint] : vFingerJoint[fb.mlJoint];
		float fAngle = fFull * fShare * mvVRFingerCurl[alHand][fb.mlFinger];
		cMatrixf mtxBend = cMath::MatrixQuaternion(cQuaternion(fAngle, fb.mvCurlAxis));
		cBoneState *pState = pEntity->GetBoneState(fb.mlBoneIdx);
		if(pState) pState->SetMatrix(cMath::MatrixMul(fb.m_mtxBind, mtxBend), true);
	}
}

//-----------------------------------------------------------------------

void cLuxPlayerHands::UpdatePlayerHandsPos(float afTimeStep)
{
	if(mpHandsEntity==NULL) return;

	cCamera *pCam = mpPlayer->GetCamera();

	// VR: the lantern is in the hand (the render callback refines this with the pose of each frame)
	UpdateVRControllers(cVRSystem::Get()->GetSeatedToWorld(pCam));
	cMatrixf mtxVRHands;
	if(GetVRHandsMatrix(cVRSystem::Get()->GetSeatedToWorld(pCam), mtxVRHands))
	{
		mlstCamRotations.clear();
		mpHandsEntity->SetMatrix(mtxVRHands);
		return;
	}

	cVector3f vRotation(pCam->GetPitch(), pCam->GetYaw(), pCam->GetRoll());
	mlstCamRotations.push_back(vRotation);
	if(mlstCamRotations.size() > mlMaxCamRotations) mlstCamRotations.pop_front();
	
	////////////////////////////////////////
	// Iterate all of the saved rotations and calculate a new
	cVector3f vFinalRot = 0;
	float fWeightTotal =0;
	float fCount = 1.0f;

	tVector3fListIt it = mlstCamRotations.begin();
	for(; it != mlstCamRotations.end(); ++it)
	{
		float fWeight = fCount / ((float)mlstCamRotations.size());
		fWeight *= fWeight; //Exp increase...
		++fCount;

		vFinalRot += *it * fWeight;
		fWeightTotal += fWeight;
	}

	vFinalRot = vFinalRot / fWeightTotal;
	
	////////////////////////////////////////
	// Set hand matrix
	cMatrixf mtxHands = cMath::MatrixRotate(vFinalRot, eEulerRotationOrder_ZXY);

	cVector3f vHandPosAdd =0;
	vHandPosAdd -= mpPlayer->GetHeadPosAdd(eLuxHeadPosAdd_Bob);
	vHandPosAdd -= mpPlayer->GetHeadPosAdd(eLuxHeadPosAdd_ScreenShake);
	vHandPosAdd -= mpPlayer->GetHeadPosAdd(eLuxHeadPosAdd_Hurt);
	///vHandPosAdd -= mpPlayer->GetHeadPosAdd(eLuxHeadPosAdd_Lean);

	cVector3f vRight = pCam->GetRight();
	cVector3f vUp = pCam->GetUp();
	cVector3f vFwd = pCam->GetForward();

	vHandPosAdd = vRight * vHandPosAdd.x + vUp * vHandPosAdd.y + vFwd * vHandPosAdd.z;

	mtxHands.SetTranslation(pCam->GetPosition() - vHandPosAdd *mfPosAddMul);

    mpHandsEntity->SetMatrix(mtxHands);
}

//-----------------------------------------------------------------------

iLuxHandObject* cLuxPlayerHands::LoadHandObject(const tString& asName)
{
	/////////////////////
	// Init variables
	cResources *pResources = gpBase->mpEngine->GetResources();

	/////////////////////
	// Get file name
	tString sFile = cString::SetFileExt(asName, "ho");
	sFile = "/models/hand_objects/"+asName+"/"+sFile;

	/////////////////////
	// Load XML document
	iXmlDocument *pDoc = pResources->LoadXmlDocument(sFile);
	if(pDoc==NULL)
	{
		Error("Could not load hand object file: '%s'\n", sFile.c_str());
		return NULL;
	}

	// Main element
	cXmlElement *pMainElem = pDoc->GetFirstElement("Main");
	if(pMainElem == NULL){
		Error("No main element found in hand object file '%s'\n", sFile.c_str());
		pResources->DestroyXmlDocument(pDoc);
		return NULL;
	}

	// Settings element
	cXmlElement *pSettingsElem = pDoc->GetFirstElement("Settings");
	if(pSettingsElem == NULL){
		Error("No settings element found in hand object file '%s'\n", sFile.c_str());
		pResources->DestroyXmlDocument(pDoc);
		return NULL;
	}


	/////////////////////
	// Create the object

	//Get the type
	eLuxHandObjectType aType = ToHandObjectType(pMainElem->GetAttributeString("Type",""));
	if(aType == eLuxHandObjectType_LastEnum)
	{
		pResources->DestroyXmlDocument(pDoc);
		return NULL;
	}

	//Create object and load data
	iLuxHandObject *pObject = CreateObjectFromType(asName,aType);
	if(aType == eLuxHandObjectType_LastEnum)
	{
		pResources->DestroyXmlDocument(pDoc);
		return NULL;
	}

	//Load the main data (model file name, etc)
	if(pObject->LoadMainData(pMainElem)==false)
	{
		pResources->DestroyXmlDocument(pDoc);
		return NULL;	
	}

	//Load the settings
	pObject->LoadSettings(pSettingsElem);


	/////////////////////
	// Add object

	mvHandObjects.push_back(pObject);

	return pObject;
}

//-----------------------------------------------------------------------

iLuxHandObject* cLuxPlayerHands::CreateObjectFromType(const tString& asName, eLuxHandObjectType aType)
{
	switch(aType)
	{
	case eLuxHandObjectType_Melee:			return hplNew(cLuxHandObject_Melee, (asName, this));
	//case eLuxHandObjectType_Ranged:			return NULL;
	case eLuxHandObjectType_LightSource:	return hplNew(cLuxHandObject_LightSource, (asName, this));
	}

	Error("HandObject '%s' of type %d could not be created! Type is not implemented in code!",asName.c_str(), aType);

	return NULL;
}

//-----------------------------------------------------------------------

eLuxHandObjectType cLuxPlayerHands::ToHandObjectType(const tString& asType)
{
	tString sLowType = cString::ToLowerCase(asType);

	if(sLowType == "melee") return eLuxHandObjectType_Melee;
	if(sLowType == "ranged") return eLuxHandObjectType_Ranged;
	if(sLowType == "lightsource") return eLuxHandObjectType_LightSource;

	Error("Hand object of type '%s' does not exist!", asType.c_str());
	return eLuxHandObjectType_LastEnum;
}

//-----------------------------------------------------------------------

