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

#include "LuxPlayer.h"
#include "LuxVR.h"
#include "LuxMoveState_Normal.h"

#include "LuxMap.h"
#include "LuxInventory.h"
#include "LuxMessageHandler.h"
#include "LuxMapHandler.h"
#include "LuxHelpFuncs.h"

#include "LuxPlayerState_Normal.h"
#include "LuxPlayerState_HandObject.h"
#include "LuxPlayerState_UseItem.h"
#include "LuxPlayerState_InteractGrab.h"
#include "LuxPlayerState_InteractPush.h"
#include "LuxPlayerState_InteractSlide.h"
#include "LuxPlayerState_InteractSwingDoor.h"
#include "LuxPlayerState_InteractLever.h"
#include "LuxPlayerState_InteractWheel.h"
#include "LuxPlayerState_Ladder.h"
#include "LuxHintHandler.h"
#include "LuxEffectHandler.h"

#include "LuxEntity.h"

#include "LuxMoveState_Normal.h"
#include "LuxMoveState_ClimbLedge.h"

#include "LuxPlayerHelpers.h"
#include "LuxPlayerHands.h"

#include "LuxAreaNodes.h"


//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPlayer::cLuxPlayer() : iLuxUpdateable("LuxPlayer"), iLuxCollideCallbackContainer()
{
	//////////////////////////////////
	// Init data pointers
	mpCharBody = NULL;

    mbUsesDragFootsteps = false;

	//////////////////////////////////
	// Create and setup camera
	mpCamera = gpBase->mpEngine->GetScene()->CreateCamera(eCameraMoveMode_Walk);

	//TODO: More setup?
	cVector2f vScreenSize = gpBase->mpEngine->GetGraphics()->GetLowLevel()->GetScreenSizeFloat();
	mfAspect = vScreenSize.x / vScreenSize.y;
	mfFOV = cMath::ToRad(gpBase->mpGameCfg->GetFloat("Player_General","FOV", 0));

	mpCamera->SetFOV(mfFOV);
	mpCamera->SetAspect(mfAspect);
	mpCamera->SetFarClipPlane(gpBase->mpGameCfg->GetFloat("Player_General","FarClipPlane",0));
	mpCamera->SetNearClipPlane(gpBase->mpGameCfg->GetFloat("Player_General","NearClipPlane",0));
	mpCamera->SetPitchLimits(-cMath::ToRad(70), cMath::ToRad(70) );

	//////////////////////////////////
	// Init General properties
	mfHeadSpinDamageSpeed = gpBase->mpGameCfg->GetFloat("Player_General","HeadSpinDamageSpeed",0);
	mfHeadSpinDeacc = gpBase->mpGameCfg->GetFloat("Player_General","HeadSpinDeacc",0);

	msTerrorSound = gpBase->mpGameCfg->GetString("Player_General","TerrorSound","");

	mfTerrorIncSpeed = gpBase->mpGameCfg->GetFloat("Player_General","TerrorIncSpeed",0);
	mfTerrorDecSpeed = gpBase->mpGameCfg->GetFloat("Player_General","TerrorDecSpeed",0);

	mlMaxPrevSpeeds = gpBase->mpGameCfg->GetInt("Player_General","MaxPrevSpeedsSaved",0);

	mfAutoKillYPos = gpBase->mpGameCfg->GetFloat("Player_General","AutoKillYPos",0);

	msHeadSpinHitSound = gpBase->mpGameCfg->GetString("Player_General","HeadSpinHitSound","");

	//////////////////////////////////
	// Init body properties
	mvBodySize = gpBase->mpGameCfg->GetVector3f("Player_Body","Size",0);
	mvBodyCrouchSize = gpBase->mpGameCfg->GetVector3f("Player_Body","CrouchSize",0);
	if(cLuxVRSettings::Get().mbActive)
	{
		float fWidth = cLuxVRSettings::Get().mfBodyWidth;
		mvBodySize.x = mvBodySize.z = fWidth;
		mvBodyCrouchSize.x = mvBodyCrouchSize.z = fWidth;
	}

	mvCameraPosAdd = gpBase->mpGameCfg->GetVector3f("Player_Body","CameraPosAdd",0);

	mfDefaultMass = gpBase->mpGameCfg->GetFloat("Player_Body","Mass",0);

	miNumberOfInfectionLevels = gpBase->mpGameCfg->GetFloat("Player_Infection","NumberOfInfectionLevels",0);

	mfInfectionLevelOneSpeedMul = gpBase->mpGameCfg->GetFloat("Player_Infection","InfectionLevelOneSpeedMultiplier",0);
	mfInfectionLevelTwoSpeedMul = gpBase->mpGameCfg->GetFloat("Player_Infection","InfectionLevelTwoSpeedMultiplier",0);
	mfInfectionLevelThreeSpeedMul = gpBase->mpGameCfg->GetFloat("Player_Infection","InfectionLevelThreeSpeedMultiplier",0);
	mfInfectionLevelFourSpeedMul = gpBase->mpGameCfg->GetFloat("Player_Infection","InfectionLevelFourSpeedMultiplier",0);

	mnMaxInfectionLevelAtWhichPlayerCanRun = gpBase->mpGameCfg->GetInt("Player_Infection","MaxInfectionLevelAtWhichPlayerCanRun",0);
	//////////////////////////////////
	// Init some other variables
	mbUsePermaDeath = gpBase->mbPTestActivated;

	mbRandomEscapeFail = true;
	mlRandomEscapeFailCount = 0;
	mbBeingChased = false;

	//////////////////////////////////
	// Create states
	mvStates.resize(eLuxPlayerState_LastEnum);

	mvStates[eLuxPlayerState_Normal] = hplNew( cLuxPlayerState_Normal, (this) );
	mvStates[eLuxPlayerState_HandObject] = hplNew( cLuxPlayerState_HandObject, (this) );
	mvStates[eLuxPlayerState_UseItem] = hplNew( cLuxPlayerState_UseItem, (this) );
	mvStates[eLuxPlayerState_InteractGrab] = hplNew( cLuxPlayerState_InteractGrab, (this) );
	mpVRSecondGrab = NULL;
	mpVRSpareGrab = hplNew( cLuxPlayerState_InteractGrab, (this) );
	mvStates[eLuxPlayerState_InteractPush] = hplNew( cLuxPlayerState_InteractPush, (this) );
	mvStates[eLuxPlayerState_InteractSlide] = hplNew( cLuxPlayerState_InteractSlide, (this) );
	mvStates[eLuxPlayerState_InteractSwingDoor] = hplNew( cLuxPlayerState_InteractSwingDoor, (this) );
	mvStates[eLuxPlayerState_InteractLever] = hplNew( cLuxPlayerState_InteractLever, (this) );
	mvStates[eLuxPlayerState_InteractWheel] = hplNew( cLuxPlayerState_InteractWheel, (this) );
	mvStates[eLuxPlayerState_Ladder] = hplNew( cLuxPlayerState_Ladder, (this) );

	//////////////////////////////////
	// Create move states
	mvMoveStates.resize(eLuxMoveState_LastEnum);

	mvMoveStates[eLuxMoveState_Normal] = hplNew( cLuxMoveState_Normal, (this) );
	mvMoveStates[eLuxMoveState_ClimbLedge] = hplNew( cLuxMoveState_ClimbLedge, (this) );

	//////////////////////////////////
	// Create head pos adds
	mvHeadPosAdds.resize(eLuxHeadPosAdd_LastEnum);


	//////////////////////////////////
	// Create graphics
	mpFocusFont = NULL;

	//////////////////////////////////
	// Create helpers
	mpHudEffect = hplNew( cLuxPlayerHudEffect, (this) );
	mvHelpers.push_back(mpHudEffect);

	mpHelperLightLevel = hplNew( cLuxPlayerLightLevel, (this) );
	mvHelpers.push_back(mpHelperLightLevel);

	//mpHelperInDarkness = hplNew( cLuxPlayerInDarkness, (this) );
	//mvHelpers.push_back(mpHelperInDarkness);

    mpHelperIsMoving = hplNew( cLuxPlayerIsMoving, (this) );
	mvHelpers.push_back(mpHelperIsMoving);

	mpLean = hplNew( cLuxPlayerLean, (this) );
	mvHelpers.push_back(mpLean);

	mpDeath = hplNew( cLuxPlayerDeath, (this) );
	mvHelpers.push_back(mpDeath);

	mpHands = hplNew( cLuxPlayerHands, (this) );
	mvHelpers.push_back(mpHands);

	mpLantern = hplNew( cLuxPlayerLantern, (this) );
	mvHelpers.push_back(mpLantern);

	mpInfection = hplNew( cLuxPlayerInfection, (this) );
	mvHelpers.push_back(mpInfection);

	mpLookAt = hplNew( cLuxPlayerLookAt, (this) );
	mvHelpers.push_back(mpLookAt);

	mpFlashback = hplNew( cLuxPlayerFlashback, (this) );
	mvHelpers.push_back(mpFlashback);

	mpHurt = hplNew( cLuxPlayerHurt, (this) );
	mvHelpers.push_back(mpHurt);

	mpSpawnPS = hplNew( cLuxPlayerSpawnPS, (this) );
	mvHelpers.push_back(mpSpawnPS);

	mpCamDirEffects = hplNew( cLuxPlayerCamDirEffects, (this));
	mvHelpers.push_back(mpCamDirEffects);

	mpInfectionCollapse = hplNew( cLuxPlayerInfectionCollapse, (this));
	mvHelpers.push_back(mpInfectionCollapse);

	mpPlayerStamina = hplNew( cLuxPlayerStamina, (this));
	mvHelpers.push_back(mpPlayerStamina);

	mpVoiceFlashback = hplNew( cLuxPlayerVoiceFlashback, (this));
	mvHelpers.push_back(mpVoiceFlashback);

	mfVomitEffectDuration = gpBase->mpGameCfg->GetInt("Player_Infection","VomitEffectDuration",1.0);
	mfTimeSinceLastVomit = mfVomitEffectDuration + 10.0f;
}

//-----------------------------------------------------------------------

cLuxPlayer::~cLuxPlayer()
{
	DestroyCollideCallbacks();

	STLDeleteAll(mvHelpers);
	STLDeleteAll(mvStates);
	if(mpVRSecondGrab) hplDelete(mpVRSecondGrab);
	hplDelete(mpVRSpareGrab);
	STLDeleteAll(mvMoveStates);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxPlayer::LoadFonts()
{
	tString sFontFile = gpBase->mpGameCfg->GetString("Player_General","FocusTextFont","");
	mpFocusFont = LoadFont(sFontFile);

	RunHelperLuxMessage(eLuxUpdateableMessage_LoadFonts,NULL);
}

//-----------------------------------------------------------------------

void cLuxPlayer::Reset()
{
	////////////////////////
	// Reset variables
	mbActive = true;

	mMoveState = eLuxMoveState_Normal;
	mvMoveStates[mMoveState]->OnEnterState(eLuxMoveState_LastEnum);

	mState = eLuxPlayerState_Normal;
	mvStates[mState]->OnEnterState(eLuxPlayerState_LastEnum);

	// The held object is gone with the old map: just drop the second grab
	if(mpVRSecondGrab)
	{
		hplDelete(mpVRSpareGrab);
		mpVRSpareGrab = mpVRSecondGrab;
		mpVRSpareGrab->SetVRSecondHand(false);
		mpVRSecondGrab = NULL;
	}

	mbPressedMove = false;
	mbPressingRun = false;
	mbPressingJump = false;

	mbJumpDisabled = false;
	mbCrouchDisabled = false;
	mbVRRealCrouch = false;
	mbVRCrouchComp = false;

	mbIsInWater = false;
	msWaterStepSound = "";
	mfWaterSpeedMul = 1.0f;

	mvHeadSpeed =0;
	mvHeadSpinSpeed =0;

	mfInteractionMoveSpeedMul = 1.0f;

	mfLookSpeedMul = 1.0f;

	mfEventMoveSpeedMul = 1.0f;
	mfEventRunSpeedMul = 1.0f;

	mfScriptMoveSpeedMul = 1.0f;
	mfScriptRunSpeedMul = 1.0f;

	mfHurtMoveSpeedMul = 1.0f;

	mfInfectionCollapseSpeedMul = 1.0f;

	mfStaminaSpeedMul = 1.0f;

	mfCurrentFocusDistance = 0;
	mpEntityInFocus = NULL;
	mpBodyInFocus = NULL;

	mpCharBody = NULL;

	msCurrentPermaDeathSound = "";

	mfHealth = 100;
	mfInfection = 0;
	mlTinderboxes =0;

	mvHeadPosAddSum =0;

	mfTerror =0;

	mlCoins =0;

	mfAvgSpeed =0;
	mlstPrevSpeeds.clear();

	mlstPrevMoveDirs.clear();
	mvAvgMoveDir2D = cVector3f(0,0,1);
	mfAddMoveDirCount=100;

	msFocusText = _W("");
	msLastFocusText = _W("");
	mfFocusTextAlpha =0;

	cSoundHandler *pSoundHandler = gpBase->mpEngine->GetSound()->GetSoundHandler();
	if(mpTerrorSound && pSoundHandler->IsValid(mpTerrorSound, mlTerrorSoundID)) mpTerrorSound->Stop();
	mpTerrorSound = NULL;
	mlTerrorSoundID = -1;

	mfAspectMul = 1.0f;
	mfFOVMul = 1.0f;
	mfAspectMulGoal = 1.0f;
	mfFOVMulGoal = 1.0f;
	mfAspectMulSpeed = 0.0f;
	mfFOVMulSpeed = 0.0f;
	mfRoll=0;
	mfRollGoal=0;
	mfRollSpeedMul=0;
	mfRollMaxSpeed=0;

    mbFadingPitch = false;
    mfPitchGoal=0;
	mfPitchSpeedMul=0;
	mfPitchMaxSpeed=0;

	mfLeanRoll=0;
	mfLeanRollGoal=0;
	mfLeanRollSpeedMul=0;
	mfLeanRollMaxSpeed=0;

	mvCamAnimPos =0;
	mvCamAnimPosGoal=0;
	mfRollSpeedMul=0;
	mfRollMaxSpeed=0;

	mbNoFallDamage = false;

	mbScriptShowFocusIconAndCrossHair = true;

	for(size_t i=0; i<mvHeadPosAdds.size(); ++i)
	{
		mvHeadPosAdds[i].mbMoving = false;
		mvHeadPosAdds[i].mvAdd = 0;
	}

	DestroyCollideCallbacks();

	m_setTerrorEnemies.clear();

	////////////////////////
	//Reset camera
	mpCamera->SetRoll(0.0f);
	mpCamera->SetPitch(0.0f);
	mpCamera->SetYaw(0.0f);
	mpCamera->SetFOV(mfFOV*mfFOVMul);
	mpCamera->SetAspect(mfAspect*mfAspectMul);

	////////////////////////
	// Reset Helpers
	RunHelperMessage(eUpdateableMessage_Reset,0);

    mbUsesDragFootsteps = false;
}

//-----------------------------------------------------------------------

void cLuxPlayer::OnStart()
{

}

//-----------------------------------------------------------------------

// VR: the body's last 64 steps, written to the log when you leave the map (to see how you got out)
struct cLuxVRBodyTrailStep { cVector3f mvPos, mvVel; bool mbOnGround; };
static cLuxVRBodyTrailStep gvVRBodyTrail[64];
static int glVRBodyTrailCount = 0;
static bool gbVRFallLogged = false;

static void LogVRBodyTrail(int alStride)
{
	for(int i = cMath::Max(glVRBodyTrailCount - 64, 0); i < glVRBodyTrailCount; i += alStride)
	{
		const cLuxVRBodyTrailStep &step = gvVRBodyTrail[i % 64];
		Log("  trail %d: feet (%.2f %.2f %.2f) force vel (%.2f %.2f %.2f) %s\n", i - glVRBodyTrailCount, step.mvPos.x, step.mvPos.y,
			step.mvPos.z, step.mvVel.x, step.mvVel.y, step.mvVel.z, step.mbOnGround ? "ground" : "air");
	}
}

void cLuxPlayer::Update(float afTimeStep)
{
	mfTimeSinceLastVomit += afTimeStep;

	////////////////////////
	// Debug (HPL_VR_TEST_WALK="x y z"): 3 s after each spawn the feet are put at x y z, then the player keeps
	// walking forward while turning in bursts (left, right, left, ...), bumping into walls and corners like a
	// player with the stick held forward. Logs the position every 2 s; an escape shows as an out-of-map kill.
	{
		static const char *pTestWalk = getenv("HPL_VR_TEST_WALK");
		static float fWalkTime = 0, fWalkNextLog = 0;
		static bool bWalkPlaced = false;
		if(pTestWalk && gpBase->mpMapHandler->GetCurrentMap())
		{
			if(IsDead()) { fWalkTime = 0; bWalkPlaced = false; }
			else
			{
				fWalkTime += afTimeStep;
				if(bWalkPlaced==false && fWalkTime > 3.0f)
				{
					cVector3f vFeet = 0;
					sscanf(pTestWalk, "%f %f %f", &vFeet.x, &vFeet.y, &vFeet.z);
					mpCharBody->SetFeetPosition(vFeet);
					bWalkPlaced = true;
					fWalkNextLog = fWalkTime + 2.0f;
					Log("VR test: walk test from %s\n", vFeet.ToString().c_str());
				}
				else if(bWalkPlaced)
				{
					int lBurst = (int)(fWalkTime / 1.7f);
					if(fmod(fWalkTime, 1.7f) < 0.6f) mpCamera->AddYaw(afTimeStep * ((lBurst % 3)==1 ? -2.5f : 2.0f));
					mpCharBody->Move(eCharDir_Forward, 1.0f);
					if(fWalkTime > fWalkNextLog)
					{
						fWalkNextLog = fWalkTime + 2.0f;
						Log("VR test: walking at %s\n", mpCharBody->GetFeetPosition().ToString().c_str());
					}
				}
			}
		}
	}

	////////////////////////
	// VR: the camera follows the head (before anything aims with it)
	UpdateVRBodyTrail();
	UpdateVRHead(afTimeStep);

	////////////////////////
	// Update current move state
	mvMoveStates[mMoveState]->Update(afTimeStep);

	////////////////////////
	// Update current state
	mvStates[mState]->Update(afTimeStep);

	////////////////////////
	// Debug (HPL_VR_TEST_HEADPOS="x y z"): a scripted head move like the cutscenes use, repeated: after 10 s the
	// head moves to the offset, 15 s later back to 0, then 10 s free, and so on. Looking around must turn in place.
	{
		static const char *pTestHeadPos = getenv("HPL_VR_TEST_HEADPOS");
		static float fTestTime = 0;
		static int lTestPhase = 0;
		if(pTestHeadPos)
		{
			fTestTime += afTimeStep;
			if(lTestPhase==0 && fTestTime > 10.0f)
			{
				cVector3f vOffset = 0;
				sscanf(pTestHeadPos, "%f %f %f", &vOffset.x, &vOffset.y, &vOffset.z);
				MoveHeadPosAdd(eLuxHeadPosAdd_Script, vOffset, 0.5f, 0.25f);
				Log("VR test: head offset to %s\n", vOffset.ToString().c_str());
				lTestPhase = 1; fTestTime = 0;
			}
			else if(lTestPhase==1 && fTestTime > 15.0f)
			{
				MoveHeadPosAdd(eLuxHeadPosAdd_Script, 0, 0.5f, 0.25f);
				Log("VR test: head offset back to 0\n");
				lTestPhase = 0; fTestTime = 0;
			}
		}
	}

	////////////////////////
	// Debug (HPL_VR_TEST_OUTOFMAP="seconds"): that long after each (re)spawn you are put 3 m beside the map's static
	// geometry at your current height, so you fall out of the map and Body OutOfMapKill must kill you.
	{
		static const char *pTestOutOfMap = getenv("HPL_VR_TEST_OUTOFMAP");
		static float fOutTime = 0;
		static bool bOutDone = false;
		if(pTestOutOfMap && gpBase->mpMapHandler->GetCurrentMap())
		{
			if(IsDead()) { fOutTime = 0; bOutDone = false; }
			else if(bOutDone==false && (fOutTime += afTimeStep) > cMath::Max((float)atof(pTestOutOfMap), 1.0f))
			{
				iPhysicsWorld *pPhysicsWorld = gpBase->mpMapHandler->GetCurrentMap()->GetPhysicsWorld();
				cVector3f vMin = pPhysicsWorld->GetWorldSizeMin(), vMax = pPhysicsWorld->GetWorldSizeMax();
				cVector3f vFeet = mpCharBody->GetFeetPosition();
				cVector3f vOut(vMax.x - 10.0f + 3.0f, vFeet.y, (vMin.z + vMax.z) * 0.5f);
				mpCharBody->SetFeetPosition(vOut);
				Log("VR test: put outside the map at %s (map x up to %.1f)\n", vOut.ToString().c_str(), vMax.x - 10.0f);
				bOutDone = true;
			}
		}
	}

	////////////////////////
	// Update head
	UpdateHeadPosAdd(afTimeStep);
	UpdateHeadSpin(afTimeStep);

	//mpCamera->AddYaw(afTimeStep * kPif);
	//mpCharBody->AddYaw(afTimeStep * kPif * 0.5f);

	////////////////////////
	// Run Helper message
	RunHelperMessage(eUpdateableMessage_Update,afTimeStep);

	////////////////////////
	//Clear some variables
	mbPressedMove = false;

	////////////////////////
	//Check if too far down
	if(mpCharBody->GetPosition().y < mfAutoKillYPos) SetHealth(0);

	////////////////////////
	//VR: out of the map (pushed through a wall or floor) = dead, so you restart at the checkpoint instead of
	//hanging stuck outside: more than 2 m below or 1 m beside the map's static geometry. The physics world is that
	//geometry's bounds + 10 m (cWorld::Compile); bodies stop moving at its edge.
	if(cLuxVRSettings::Get().mbActive && cLuxVRSettings::Get().mbOutOfMapKill && IsDead()==false && mpDeath->IsActive()==false &&
		gpBase->mpMapHandler->GetCurrentMap())
	{
		iPhysicsWorld *pPhysicsWorld = gpBase->mpMapHandler->GetCurrentMap()->GetPhysicsWorld();
		cVector3f vMin = pPhysicsWorld->GetWorldSizeMin() + cVector3f(9, 8, 9);
		cVector3f vMax = pPhysicsWorld->GetWorldSizeMax() - cVector3f(9, 0, 9);
		cVector3f vPos = mpCharBody->GetPosition();
		if(vMax.x > vMin.x && vMax.z > vMin.z &&
			(vPos.y < vMin.y || vPos.x < vMin.x || vPos.x > vMax.x || vPos.z < vMin.z || vPos.z > vMax.z))
		{
			Log("VR: player out of the map at (%.1f %.1f %.1f), map (%.1f %.1f %.1f)-(%.1f %.1f %.1f): killed\n",
				vPos.x, vPos.y, vPos.z, vMin.x, vMin.y, vMin.z, vMax.x, vMax.y, vMax.z);
			LogVRBodyTrail(2);
			SetHealth(0);
		}
	}

	////////////////////////
	// Update misc
	CheckCollisionCallback("Player", gpBase->mpMapHandler->GetCurrentMap());
	UpdateCamera(afTimeStep);
	UpdateTerror(afTimeStep);
	UpdateFocusText(afTimeStep);
	UpdateAvgSpeed(afTimeStep);
	UpdateAvgMoveDir(afTimeStep);
}

//-----------------------------------------------------------------------

void cLuxPlayer::PostUpdate(float afTimeStep)
{
	////////////////////////
	// Run Helper message
	RunHelperMessage(eUpdateableMessage_PostUpdate,afTimeStep);

	////////////////////////
	// Post Update current state
	mvStates[mState]->PostUpdate(afTimeStep);

	UpdateVRSecondGrab(afTimeStep);
}

//-----------------------------------------------------------------------

void cLuxPlayer::OnDraw(float afFrameTime)
{
	////////////////////////
	// Draw with current move state
	mvMoveStates[mMoveState]->OnDraw(afFrameTime);

	////////////////////////
	// Draw with current state
	mvStates[mState]->OnDraw(gpBase->mpGameHudSet, afFrameTime);

	////////////////////////
	// Run Helper message
	RunHelperMessage(eUpdateableMessage_OnDraw,afFrameTime);

	////////////////////////
	// Draw player HUD
	DrawHud(afFrameTime);
}

//-----------------------------------------------------------------------

void cLuxPlayer::LoadUserConfig()
{
	mbShowCrossHair = gpBase->mpUserConfig->GetBool("Game", "ShowCrossHair", true);
	mFocusIconStyle = StringToFocusIconStyle(gpBase->mpUserConfig->GetString("Game", "FocusIconStyle", "Default"));

	RunHelperLuxMessage(eLuxUpdateableMessage_LoadUserConfig,0);
}

void cLuxPlayer::SaveUserConfig()
{
	gpBase->mpUserConfig->SetBool("Game", "ShowCrossHair", mbShowCrossHair);
	gpBase->mpUserConfig->SetString("Game", "FocusIconStyle", FocusIconStyleToString(mFocusIconStyle));

	RunHelperLuxMessage(eLuxUpdateableMessage_SaveUserConfig,0);
}

//-----------------------------------------------------------------------

void cLuxPlayer::OnMapEnter(cLuxMap *apMap)
{
	////////////////////////////////
	//Init all move states
	for(int i=0; i<eLuxMoveState_LastEnum; ++i) mvMoveStates[i]->OnMapEnter();


	////////////////////////////////
	//Init current move state
	mvMoveStates[mMoveState]->OnEnterState(mMoveState);

	////////////////////////////////
	//Call current state
	mvStates[mState]->OnMapEnter(apMap);

	////////////////////////
	// Run Helper message
	RunHelperLuxMessage(eLuxUpdateableMessage_OnMapEnter, apMap);

	////////////////////////////////
	//Reset focus text
	msFocusText = _W("");
	msLastFocusText = _W("");
	mfFocusTextAlpha =0;

	////////////////////////////////
	//Reset variables
	mbIsInWater = false;
	mpEntityInFocus = NULL;
	mpBodyInFocus = NULL;
	mlstPrevMoveDirs.clear();
	mvAvgMoveDir2D = cVector3f(0,0,1);
	mfAddMoveDirCount=100;
}

//-----------------------------------------------------------------------

void cLuxPlayer::OnMapLeave(cLuxMap *apMap)
{
	ReleaseVRSecondGrab();

	////////////////////////
	// Set default state
	ChangeState(eLuxPlayerState_Normal);

	////////////////////////////////
	//Call current state
	mvStates[mState]->OnMapLeave(apMap);

	////////////////////////
	// Clear collide callbacks
	DestroyCollideCallbacks();

	////////////////////////
	// Run Helper message
	RunHelperLuxMessage(eLuxUpdateableMessage_OnMapLeave, apMap);

	/////////////////////
	// Reset stuff
	m_setTerrorEnemies.clear();

	mfAspectMulGoal = 1.0f;
	mfAspectMul = 1.0f;
	mfFOVMulGoal = 1.0f;
	mfFOVMul = 1.0f;
	mfRollGoal=0;
	mfRoll=0;

    mbFadingPitch = false;
    mfPitchGoal=0;
	mfPitchSpeedMul=0;
	mfPitchMaxSpeed=0;

	mfLeanRoll=0;
	mfLeanRollGoal=0;
	mfLeanRollSpeedMul=0;
	mfLeanRollMaxSpeed=0;

	mpCamera->SetRoll(0.0f);
	mpCamera->SetFOV(mfFOV*mfFOVMul);
	mpCamera->SetAspect(mfAspect*mfAspectMul);
}

//-----------------------------------------------------------------------

void cLuxPlayer::CreateWorldEntities(cLuxMap *apMap)
{
	////////////////////////////////
	//Create and Init character body
	CreateCharacterBody(apMap->GetPhysicsWorld());

	////////////////////////
	// Run Helper message
	for(size_t i=0; i<mvHelpers.size(); ++i) mvHelpers[i]->CreateWorldEntities(apMap);
}

void cLuxPlayer::DestroyWorldEntities(cLuxMap *apMap)
{
	////////////////////////////////
	//Destroy character body
	if(mpCharBody)
	{
		apMap->GetPhysicsWorld()->DestroyCharacterBody(mpCharBody);
		mpCharBody = NULL;
	}

	////////////////////////
	// Run Helper message
	for(size_t i=0; i<mvHelpers.size(); ++i) mvHelpers[i]->DestroyWorldEntities(apMap);
}
//-----------------------------------------------------------------------

void cLuxPlayer::OnEnterContainer(const tString& asOldContainer)
{

}

void cLuxPlayer::OnLeaveContainer(const tString& asNewContainer)
{
	//Reset some variables
	mbPressingRun = false;
	mbPressingJump = false;

	//Do not be in an interaction mode
	ReleaseVRSecondGrab();
	if(	mState == eLuxPlayerState_InteractGrab ||
		mState == eLuxPlayerState_InteractSlide ||
		mState == eLuxPlayerState_InteractPush ||
		mState == eLuxPlayerState_InteractWheel ||
		mState == eLuxPlayerState_InteractLever)
	{
		ChangeState(eLuxPlayerState_Normal);
	}
}

//-----------------------------------------------------------------------

void cLuxPlayer::RenderSolid(cRendererCallbackFunctions* apFunctions)
{
	mvStates[mState]->RenderSolid(apFunctions);

	for(size_t i=0; i<mvHelpers.size(); ++i) mvHelpers[i]->RenderSolid(apFunctions);
}

void cLuxPlayer::RenderTrans(cRendererCallbackFunctions* apFunctions)
{
	mvStates[mState]->RenderTrans(apFunctions);
}


//-----------------------------------------------------------------------

void cLuxPlayer::GiveDamage(float afAmount, int alStrength, eLuxDamageType aType, bool abSpinHead, bool abLethal)
{
	if(mfHealth <=0) return;

	mfHealth -= afAmount;

	if(mState==eLuxPlayerState_Ladder) ChangeState(eLuxPlayerState_Normal);

	mpHudEffect->AddDamageSplash(aType);
	mpHudEffect->Flash(cColor(0.6f,0,0, 0.5f),eGuiMaterial_Alpha,0,0.25f);
	if(abSpinHead) SpinHead(mfHeadSpinDamageSpeed);

	//VR: the head spin (the flat game's big "you got hit" cue) is off for comfort, so both controllers buzz instead
	if(cLuxVRSettings::Get().mbActive && cLuxVRSettings::Get().mbHaptics && cVRSystem::Get())
	{
		cVRSystem::Get()->TriggerHaptic(eVRHand_Left, 0.25f, 60.0f, 1.0f);
		cVRSystem::Get()->TriggerHaptic(eVRHand_Right, 0.25f, 60.0f, 1.0f);
	}

	if(abSpinHead)
		gpBase->mpHelpFuncs->PlayGuiSoundData(msHeadSpinHitSound, eSoundEntryType_Gui);

	if(abLethal==false && mfHealth < 10)
	{
		mfHealth = 10;
	}

	if(mfHealth <=0)
	{
		mpDeath->Start();
	}
}

//-----------------------------------------------------------------------

void cLuxPlayer::GiveInfectionDamage(float afAmount)
{
	gpBase->mpHintHandler->Add("InfectionHit", kTranslate("Hints", "InfectionHit"), 0);

	IncreaseInfection(afAmount, true);

	mpInfection->StartHit();
}

void cLuxPlayer::IncreaseInfection(float afAmount, bool abUseEffect)
{
	if(mfHealth <=0) return;

	mfInfection += afAmount;
	if(mfInfection >= 100.0f)
	{
		mfInfection = 100.0f;
		//mpInfectionCollapse->Start();
	}

	if(abUseEffect)
	{
		mpInfection->StartInfectionIncreaseEffects();
	}
}

//-----------------------------------------------------------------------

void cLuxPlayer::Move(eCharDir aDir, float afMul)
{
	mbPressedMove = true;
	if(mvStates[mState]->OnMove(aDir, afMul))
	{
		if(mvMoveStates[mMoveState]->OnMove(aDir, afMul))
		{
			mpCharBody->Move(aDir, afMul);
		}
	}
}

//-----------------------------------------------------------------------

void cLuxPlayer::AddYaw(float afAmount)
{
	afAmount = afAmount * mfLookSpeedMul;

	if(mvStates[mState]->OnAddYaw(afAmount))
	{
		if(mvMoveStates[mMoveState]->OnAddYaw(afAmount))
		{
			//must do like this or else limits will fail!
			mpCamera->AddYaw( mpCamDirEffects->AddAndGetYawAdd(afAmount * 2.0f) );
			mpCharBody->SetYaw(mpCamera->GetYaw());
		}
	}
}

void cLuxPlayer::AddPitch(float afAmount)
{
	afAmount = afAmount * mfLookSpeedMul;

	if(mvStates[mState]->OnAddPitch(afAmount))
	{
		if(mvMoveStates[mMoveState]->OnAddPitch(afAmount))
		{
			// VR: the head gives the pitch (the states above still get the input, e.g. for levers)
			if(cVRSystem::Get()->IsActive()==false)
				mpCamera->AddPitch( mpCamDirEffects->AddAndGetPitchAdd(afAmount * 2.0f) );
		}
	}
}

//-----------------------------------------------------------------------

void cLuxPlayer::Lean(float afMul)
{
	mpLean->Lean(afMul);
}

//-----------------------------------------------------------------------

void cLuxPlayer::Scroll(float afAmount)
{
	mvStates[mState]->OnScroll(afAmount);
}

//-----------------------------------------------------------------------


void cLuxPlayer::DoAction(eLuxPlayerAction aAction, bool abPressed)
{
	if(mvStates[mState]->OnDoAction(aAction, abPressed))
	{
		if(aAction== eLuxPlayerAction_Lantern && abPressed)
		{
			mpLantern->SetActive(!mpLantern->IsActive(), true);
		}

	}
}

//-----------------------------------------------------------------------


void cLuxPlayer::Run(bool abPressed)
{
	mbPressingRun = abPressed;
	if(mvStates[mState]->OnRun(abPressed))
	{
		mvMoveStates[mMoveState]->OnRun(abPressed);
	}
}

void cLuxPlayer::Jump(bool abPressed)
{
	if(abPressed && (mbJumpDisabled || mpInfectionCollapse->IsActive()) ) return;

	mbPressingJump = abPressed;
	if(mvStates[mState]->OnJump(abPressed))
	{
		mvMoveStates[mMoveState]->OnJump(abPressed);
	}
}

void cLuxPlayer::Crouch(bool abPressed)
{
	if(abPressed && (mbCrouchDisabled || mpInfectionCollapse->IsActive()) ) return;

	if(mvStates[mState]->OnCrouch(abPressed))
	{
		mvMoveStates[mMoveState]->OnCrouch(abPressed);
	}
}

//-----------------------------------------------------------------------

void cLuxPlayer::ChangeState(eLuxPlayerState aState)
{
	if(mState == aState) return;

	// VR hands: objects are picked up with the grip (not the trigger), and only when the controller touches them
	eLuxVRInteractSource vrSource = LuxVRInteractSource();
	if(aState == eLuxPlayerState_InteractGrab && vrSource != eLuxVRInteractSource_None)
	{
		if(vrSource != eLuxVRInteractSource_Grip) return;
		if(mfCurrentFocusDistance > kfLuxVRRayBack + cLuxVRSettings::Get().mfGrabReach) return;
	}
	// ...and doors, levers, wheels, drawers and pushed objects (trigger or grip) only when the controller touches them
	if((aState == eLuxPlayerState_InteractSwingDoor || aState == eLuxPlayerState_InteractLever || aState == eLuxPlayerState_InteractWheel ||
		aState == eLuxPlayerState_InteractSlide || aState == eLuxPlayerState_InteractPush) && vrSource != eLuxVRInteractSource_None)
	{
		if(mfCurrentFocusDistance > kfLuxVRRayBack + cLuxVRSettings::Get().mfInteractReach) return;
	}

	// VR: grabbing what the other hand holds takes it out of that hand
	if(aState == eLuxPlayerState_InteractGrab && mpVRSecondGrab && mpVRSecondGrab->GetCurrentProp() == cLuxPlayerStateVars::mpInteractProp)
		ReleaseVRSecondGrab();

	mvStates[mState]->OnLeaveState(aState);

	eLuxPlayerState prevState = mState;
	mState = aState;

	mvStates[mState]->SetPreviousState(prevState);
	mvStates[mState]->OnEnterState(prevState);
}

//-----------------------------------------------------------------------

void cLuxPlayer::ChangeMoveState(eLuxMoveState aState)
{
	if(mMoveState == aState) return;

	mvMoveStates[mMoveState]->OnLeaveState(aState);

	eLuxMoveState prevState = mMoveState;
	mMoveState = aState;

	mvMoveStates[mMoveState]->OnEnterState(prevState);
}

//-----------------------------------------------------------------------

void cLuxPlayer::MoveHeadPosAdd(eLuxHeadPosAdd aType, const cVector3f& avGoal, float afSpeed, float afSlowdownDist)
{
	mvHeadPosAdds[aType].mbMoving = true;
	mvHeadPosAdds[aType].mvMoveGoal = avGoal;
	mvHeadPosAdds[aType].mfMoveSpeed = afSpeed;
	mvHeadPosAdds[aType].mfMoveSlowDownDist = afSlowdownDist;
}

//-----------------------------------------------------------------------

void cLuxPlayer::PlaceAtStartNode(cLuxNode_PlayerStart *apNode)
{
	ChangeState(eLuxPlayerState_Normal);

	SetIsInWater(false);//Needs to rest this or else player will spalsh when walking on the ground.
	if(mpSpawnPS->IsActive())
		mpSpawnPS->RespawnAll(); //Needs to respawn all!

	mpCharBody->SetFeetPosition(apNode->GetPosition());
	mpCharBody->SetYaw(apNode->GetAngle());
	mpCamera->SetYaw(apNode->GetAngle());
	mpCamera->SetPitch(0);
	mpCharBody->Update(0.001f);
}

//-----------------------------------------------------------------------

void cLuxPlayer::SetHeadPosAdd(eLuxHeadPosAdd aType, const cVector3f& avVector)
{
	mvHeadPosAdds[aType].mvAdd = avVector;
}

const cVector3f& cLuxPlayer::GetHeadPosAdd(eLuxHeadPosAdd aType)
{
	return mvHeadPosAdds[aType].mvAdd;
}

//-----------------------------------------------------------------------

void cLuxPlayer::SetActive(bool abX)
{
	mbActive = abX;
}

//-----------------------------------------------------------------------

void cLuxPlayer::SetHealth(float afX)
{
	if(mfHealth <=0 && afX <= 0) return;

	mfHealth = afX;

	if(mfHealth <=0)
	{
		mpDeath->Start();
	}
}

void cLuxPlayer::SetInfection(float afX, bool abShowEffect)
{
	mfInfection = afX;

	if (mfInfection < 0) mfInfection = 0;

	if ( mfInfection >= 100.0f )
	{
		mfInfection = 100.0f;
		//mpInfectionCollapse->Start();
	}

    if ( abShowEffect )
    {
        if ( afX < 0 )
        {
//		    gpBase->mpEffectHandler->GetInfectionHealFlash()->Start();
        }
        else if ( afX > 0 )
        {
            mpInfection->StartInfectionIncreaseEffects();
        }
    }
}

void cLuxPlayer::SetInfectionLevel( int aiInfectionLevel )
{
	SetInfection( GetInfectionForInfectionLevel( aiInfectionLevel ) );
}

int cLuxPlayer::GetInfectionLevel()
{
	return GetInfectionLevelForInfection( mfInfection );
}

bool cLuxPlayer::IsAtMaxInfectionLevel()
{
	return GetInfectionLevel() == miNumberOfInfectionLevels;
}

int cLuxPlayer::GetInfectionLevelForInfection( float afInfection )
{
	return afInfection < 0.001f ? 0 : 1 + (int) ( miNumberOfInfectionLevels * ( (int) ( mfInfection - 0.001f ) ) ) / 100;
}

float cLuxPlayer::GetInfectionForInfectionLevel( int aiInfectionLevel )
{
	return aiInfectionLevel == 0 ? 0.0f : ( aiInfectionLevel + 0.5f ) * 100.0f / (miNumberOfInfectionLevels + 1);
}

//-----------------------------------------------------------------------

void cLuxPlayer::AddHealth(float afX)
{
	if( (mfHealth >= 100 && afX>0) || mfHealth<0) return;

	mfHealth += afX;
	if(mfHealth > 100) mfHealth = 100;

	if(mfHealth <=0)
	{
		mpDeath->Start();
	}
}

void cLuxPlayer::AddInfection(float afX, bool abShowEffect)
{
	if (mfInfection <= 0 && afX<0) return;

	mfInfection += afX;

	if (mfInfection < 0) mfInfection = 0;

	if ( mfInfection >= 100.0f )
	{
		mfInfection = 100.0f;
		//mpInfectionCollapse->Start();
	}

	if ( afX < 0 && abShowEffect )
	{
		//gpBase->mpEffectHandler->GetInfectionHealFlash()->Start();
	}
}

void cLuxPlayer::VomitDamage()
{
	mfTimeSinceLastVomit = 0.0f;

	/*if ( GetInfectionLevel() < 3 )
	{
		SetInfectionLevel( 3 );
	}
	else*/ if ( GetInfectionLevel() < 4 )
	{
		SetInfectionLevel( 4 );
	}
}

//-----------------------------------------------------------------------

void cLuxPlayer::AddTerrorEnemy(iLuxEnemy *apEnemy)
{
	m_setTerrorEnemies.insert(apEnemy);
}

void cLuxPlayer::RemoveTerrorEnemy(iLuxEnemy *apEnemy)
{
	m_setTerrorEnemies.erase(apEnemy);
}

void cLuxPlayer::ClearTerrorEnemies()
{
	m_setTerrorEnemies.clear();
}

void cLuxPlayer::StopTerrorSound()
{
	if(mpTerrorSound && gpBase->mpEngine->GetSound()->GetSoundHandler()->IsValid(mpTerrorSound, mlTerrorSoundID))
	{
		mpTerrorSound->Stop();
		mpTerrorSound = NULL;
		mlTerrorSoundID = -1;
	}
}

//-----------------------------------------------------------------------

void cLuxPlayer::SetFocusText(const tWString &asText)
{
	if(msFocusText == asText) return;

	msLastFocusText = msFocusText;
	msFocusText = asText;
}

//-----------------------------------------------------------------------

void cLuxPlayer::AddCoins(int alX)
{
	mlCoins += alX;
}

//-----------------------------------------------------------------------

void cLuxPlayer::SetCurrentHandObjectDrawn(bool abX)
{
	cLuxInventory_Item *pItem = gpBase->mpInventory->GetEquippedHandItem();
	if(pItem == NULL) return;

	////////////////
	// Draw
    if(abX)
	{
		mpHands->SetActiveHandObject(pItem->GetSubType());
	}
	////////////////
	// Holster
	else
	{
		mpHands->SetActiveHandObject("");
	}
}

//-----------------------------------------------------------------------

void cLuxPlayer::FadeFOVMulTo(float afX, float afSpeed)
{
	mfFOVMulGoal = afX;
	mfFOVMulSpeed = afSpeed;
}

void cLuxPlayer::FadeAspectMulTo(float afX, float afSpeed)
{
	mfAspectMulGoal = afX;
	mfAspectMulSpeed = afSpeed;
}

void cLuxPlayer::FadeRollTo(float afX, float afSpeedMul, float afMaxSpeed)
{
	mfRollGoal = afX;
	mfRollSpeedMul = afSpeedMul;
	mfRollMaxSpeed = afMaxSpeed;
}

void cLuxPlayer::FadePitchTo(float afX, float afSpeedMul, float afMaxSpeed)
{
    float fPitchLimitMin = mpCamera->GetPitchMinLimit();
    float fPitchLimitMax = mpCamera->GetPitchMaxLimit();

    if(fPitchLimitMin!=0 || fPitchLimitMax!=0)
	{
		if(afX > fPitchLimitMax) afX = fPitchLimitMax;
		if(afX < fPitchLimitMin) afX = fPitchLimitMin;
	}

	mfPitchGoal = afX;
	mfPitchSpeedMul = afSpeedMul;
	mfPitchMaxSpeed = afMaxSpeed;
    mbFadingPitch = true;
}

void cLuxPlayer::FadeLeanRollTo(float afX, float afSpeedMul, float afMaxSpeed)
{
	mfLeanRollGoal = afX;
	mfLeanRollSpeedMul = afSpeedMul;
	mfLeanRollMaxSpeed = afMaxSpeed;
}

void cLuxPlayer::SetRoll(float afX)
{
	mfRoll = afX;
	mfRollGoal = afX;

}

//-----------------------------------------------------------------------

void cLuxPlayer::StartRandomEscapeFail()
{
	if(mbRandomEscapeFail && cMath::RandRectf(0, 1) < 0.33f)
	{
		mbRandomEscapeFail = false;
		mlRandomEscapeFailCount = 3;
	}
}

void cLuxPlayer::EndRandomEscapeFail()
{
	////////////
	// If the player did not try to press escape let it happen again
	if(mlRandomEscapeFailCount == 3)
	{
		mbRandomEscapeFail = true;
	}

	mlRandomEscapeFailCount = 0;
}


//-----------------------------------------------------------------------

eLuxFocusIconStyle cLuxPlayer::StringToFocusIconStyle(const tString& asX)
{
	tString sLowCase = cString::ToLowerCase(asX);

	if(sLowCase=="default")
		return eLuxFocusIconStyle_Default;
	else if(sLowCase=="simple")
		return eLuxFocusIconStyle_Simple;
	else
		Log("Error converting string to FocusIconStyle : '%s'\n", asX.c_str());

	return eLuxFocusIconStyle_Default;
}

tString cLuxPlayer::FocusIconStyleToString(eLuxFocusIconStyle aX)
{
	if(aX==eLuxFocusIconStyle_Default)
		return "Default";
	else if(aX==eLuxFocusIconStyle_Simple)
		return "Simple";

	return "Default";
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

bool cLuxPlayer::CanDrawCrossHair()
{
	if(gpBase->mpMessageHandler->IsPauseMessageActive()) return false;
	if(mbScriptShowFocusIconAndCrossHair==false) return false;
	if(mbShowCrossHair==false && mState != eLuxPlayerState_UseItem)
	{
		if(mpEntityInFocus)
		{
			if(mpEntityInFocus->CanInteract(mpBodyInFocus) && mfCurrentFocusDistance <= mpEntityInFocus->GetMaxFocusDistance())
				return true;
			else
				return false;
		}
		else
		{
			return false;
		}
	}
	return true;
}

//-----------------------------------------------------------------------

void cLuxPlayer::DrawHud(float afFrameTime)
{
	cVector2f vSetSize = gpBase->mvHudVirtualCenterSize;

	/////////////////////////////////////////
	// Cross hair
	if(CanDrawCrossHair())
	{
        cGuiGfxElement *pCrossGfx = mvStates[mState]->GetCrosshair();
		if(pCrossGfx && gpBase->mpCrosshairSet != gpBase->mpGameHudSet)
		{
			// VR reticle: the crosshair set is placed (relative to the rendered head) at the aimed spot, scaled
			// with the distance so it keeps the same size to the eye. Distance = last interaction ray hit.
			const cLuxVRSettings &vrSettings = cLuxVRSettings::Get();
			float fDist = cMath::Clamp(mfCurrentFocusDistance, vrSettings.mfReticleMinDist, vrSettings.mfReticleMaxDist);
			cVector2f vSize = cVector2f(vrSettings.mfReticleSize, vrSettings.mfReticleSize * vSetSize.y / vSetSize.x);
			gpBase->mpCrosshairSet->Set3DSize(cVector3f(vSize.x, vSize.y, 0.001f));
			// With a pointing hand: at the hand ray's hit point (the set is laid out for 1 m and pushed out to it)
			cVector3f vRayStart, vRayDir;
			bool bHandRay = LuxVRGetPointerRay(mpCamera, vRayStart, vRayDir);
			gpBase->mpCrosshairSet->SetVRWorldTarget(bHandRay, vRayStart + vRayDir*fDist);
			if(bHandRay) fDist = 1.0f;
			cMatrixf mtxReticle = cMath::MatrixScale(cVector3f(fDist));
			mtxReticle.SetTranslation(cVector3f(-vSize.x*0.5f*fDist, vSize.y*0.5f*fDist, -fDist));
			gpBase->mpCrosshairSet->Set3DTransform(mtxReticle);
		}
		if(pCrossGfx)
		{
			cVector2f vGfxSize = pCrossGfx->GetImageSize();
			cVector2f vPos = (vSetSize - vGfxSize)/2.0f;
			cVector3f vFinalPos = cVector3f(vPos.x, vPos.y,1);

			if(mvStates[mState]->OnDrawCrossHair(pCrossGfx,vFinalPos, vGfxSize))
			{
				//Log("Drawcrosshair: %p\n", pCrossGfx);
				gpBase->mpCrosshairSet->DrawGfx(pCrossGfx,vFinalPos,vGfxSize,cColor(1,1));
			}
		}
	}

	/////////////////////////////////////////
	// Focus text
	float fFocusTextY = 450;
	if(msFocusText != _W(""))
	{
		tWStringVec vRows;
		mpFocusFont->GetWordWrapRows(500, 28, 28, msFocusText,&vRows);

		for(size_t i=0; i<vRows.size(); ++i)
			gpBase->mpGameHudSet->DrawFont(vRows[i],mpFocusFont,cVector3f(400, fFocusTextY+i*24,1),28,cColor(1,mfFocusTextAlpha),	eFontAlign_Center);
	}
	else if(mfFocusTextAlpha >0)
	{
		tWStringVec vRows;
		mpFocusFont->GetWordWrapRows(500, 28, 28, msLastFocusText,&vRows);

		for(size_t i=0; i<vRows.size(); ++i)
			gpBase->mpGameHudSet->DrawFont(vRows[i],mpFocusFont,cVector3f(400, fFocusTextY+i*24,1),28,cColor(1,mfFocusTextAlpha),	eFontAlign_Center);
	}
}

//-----------------------------------------------------------------------

void cLuxPlayer::RunHelperMessage(eUpdateableMessage aMessage, float afX)
{
	for(size_t i=0; i<mvHelpers.size(); ++i)
	{
		iLuxPlayerHelper *pHelper = mvHelpers[i];
		pHelper->RunMessage(aMessage, afX);
	}
}

//-----------------------------------------------------------------------

void cLuxPlayer::RunHelperLuxMessage(eLuxUpdateableMessage aMessage, void *apData)
{
	for(size_t i=0; i<mvHelpers.size(); ++i)
	{
		iLuxPlayerHelper *pHelper = mvHelpers[i];
		pHelper->LuxRunMessage(aMessage, apData);
	}
}

//-----------------------------------------------------------------------

void cLuxPlayer::UpdateHeadPosAdd(float afTimeStep)
{
	cVector3f vAdd = mvCameraPosAdd;

	// VR: head bob and screen shake move the whole view; scale them down (vr_settings.cfg Comfort)
	bool bVR = cVRSystem::Get()->IsActive();
	const cLuxVRSettings &vrSettings = cLuxVRSettings::Get();

	/////////////////////////
	// Iterate different head offsets and add
    for(size_t i=0; i<mvHeadPosAdds.size(); ++i)
	{
		cLuxHeadPosAdd *pPosAdd = &mvHeadPosAdds[i];

		/////////////////////
		// Movement
		if(pPosAdd->mbMoving)
		{
			cVector3f vDir = pPosAdd->mvMoveGoal - pPosAdd->mvAdd;
			float fDist = vDir.Length();
			vDir = vDir / fDist;

			//If close enough to goal, slow down.
            float fSpeed = pPosAdd->mfMoveSpeed;
			if(fDist < pPosAdd->mfMoveSlowDownDist)
			{
				fSpeed *= fDist / pPosAdd->mfMoveSlowDownDist;
			}

			pPosAdd->mvAdd += vDir * fSpeed * afTimeStep;

			if(fDist < 0.001f)
			{
				pPosAdd->mbMoving = false;
				pPosAdd->mvAdd = pPosAdd->mvMoveGoal;
			}
		}

		float fMul = 1;
		if(bVR && i==eLuxHeadPosAdd_Bob) fMul = vrSettings.mbHeadBob ? 1.0f : 0.0f;
		if(bVR && i==eLuxHeadPosAdd_ScreenShake) fMul = vrSettings.mfScreenShake;
		vAdd += pPosAdd->mvAdd * fMul;
	}
	mvHeadPosAddSum = vAdd - mvCameraPosAdd;

	// VR: the character body turns the sideways/forward part with the body yaw, which follows the head, so the view
	// circled around the body when looking around (e.g. the 01_mansion_01 intro leaves (-0.2,-0.3,-0.3) that takes
	// minutes to return to 0, and saves keep it). Turn it back so it follows the play space instead of the head:
	// scripted moves still happen, but turning the head turns in place.
	if(bVR)
	{
		cVector3f vLocal = mvHeadPosAddSum;
		cVector3f vTurned = cMath::MatrixMul(cMath::MatrixRotateY(-cVRSystem::Get()->GetConsumedHeadYaw()), cVector3f(vLocal.x, 0, -vLocal.z));
		vAdd = mvCameraPosAdd + cVector3f(vTurned.x, vLocal.y, -vTurned.z);
		cVRSystem::Get()->SetDiagHeadPosAdd(vAdd);
	}


	mpCharBody->SetCameraPosAdd(vAdd);
}

//-----------------------------------------------------------------------

void cLuxPlayer::UpdateCamera(float afTimeStep)
{
	////////////////
	// FOV
	if(mfFOVMul != mfFOVMulGoal)
	{
		float fSpeed = (mfFOVMulGoal - mfFOVMul) * mfFOVMulSpeed;
		mfFOVMul += afTimeStep * fSpeed;

		if(cMath::Abs(mfFOVMulGoal - mfFOVMul) < 0.004f)
			mfFOVMul = mfFOVMulGoal;

		mpCamera->SetFOV(mfFOV*mfFOVMul);
	}

	////////////////
	// Aspect
	if(mfAspectMul != mfAspectMulGoal)
	{
		float fSpeed = (mfAspectMulGoal - mfAspectMul) * mfAspectMulSpeed;
		mfAspectMul += afTimeStep * fSpeed;

		if(cMath::Abs(mfAspectMulGoal - mfAspectMul) < 0.004f) mfAspectMul = mfAspectMulGoal;

		mpCamera->SetAspect(mfAspect*mfAspectMul);
	}

	bool bUpdatedRoll = false;
	////////////////
	// Roll
	if(mfRoll != mfRollGoal)
	{
		float fSpeed = (mfRollGoal - mfRoll) * mfRollSpeedMul;
		if(fSpeed > mfRollMaxSpeed) fSpeed = mfRollMaxSpeed;
		if(fSpeed < -mfRollMaxSpeed) fSpeed = -mfRollMaxSpeed;

		mfRoll += afTimeStep * fSpeed;

		if(cMath::Abs(mfRollGoal - mfRoll) < 0.004f) mfRoll = mfRollGoal;

		bUpdatedRoll = true;
	}

    ////////////////
	// Pitch
	if(mbFadingPitch)
	{
		float fSpeed = (mfPitchGoal - mpCamera->GetPitch()) * mfPitchSpeedMul;
		if(fSpeed > mfPitchMaxSpeed) fSpeed = mfPitchMaxSpeed;
		if(fSpeed < -mfPitchMaxSpeed) fSpeed = -mfPitchMaxSpeed;

        mpCamera->SetPitch( mpCamera->GetPitch() + afTimeStep * fSpeed );

		if(cMath::Abs(mfPitchGoal - mpCamera->GetPitch()) < 0.004f)
        {
            mpCamera->SetPitch( mfPitchGoal );
            mbFadingPitch = false;
        }
	}

	////////////////
	// Lean Roll
	if(mfLeanRoll != mfLeanRollGoal)
	{
		float fSpeed = (mfLeanRollGoal - mfLeanRoll) * mfLeanRollSpeedMul;
		if(fSpeed > mfLeanRollMaxSpeed) fSpeed = mfLeanRollMaxSpeed;
		if(fSpeed < -mfLeanRollMaxSpeed) fSpeed = -mfLeanRollMaxSpeed;

		mfLeanRoll += afTimeStep * fSpeed;

		if(cMath::Abs(mfLeanRollGoal - mfLeanRoll) < 0.004f) mfLeanRoll = mfLeanRollGoal;

		bUpdatedRoll = true;
	}

	if(bUpdatedRoll)
		mpCamera->SetRoll(mfRoll + mfLeanRoll);

	////////////////
	// Cam pos
	if(mvCamAnimPos != mvCamAnimPosGoal)
	{
		cVector3f vDir = mvCamAnimPosGoal - mvCamAnimPos;
		float fSpeed = vDir.Length() * mfCamAnimPosSpeedMul;
		if(fSpeed > mfCamAnimPosMaxSpeed) fSpeed = mfCamAnimPosMaxSpeed;
		vDir.Normalize();

        mvCamAnimPos += vDir * fSpeed;
	}

}

//-----------------------------------------------------------------------

void cLuxPlayer::UpdateTerror(float afTimeStep)
{
	if(m_setTerrorEnemies.empty() || IsDead())
	{
		mfTerror -= mfTerrorDecSpeed * afTimeStep;
		if(mfTerror < 0) mfTerror =0;
	}
	else
	{
		float fSpeed = mpCharBody->GetVelocity(afTimeStep).Length();

		mfTerror += mfTerrorIncSpeed * afTimeStep; //+ fSpeed * afTimeStep * 0.1f;
		if(mfTerror > 1) mfTerror = 1;
	}


	cSoundHandler *pSoundHandler = gpBase->mpEngine->GetSound()->GetSoundHandler();

	if(mfTerror > 0)
	{
		if(mpTerrorSound == NULL)
		{
			mpTerrorSound = pSoundHandler->PlayGui(msTerrorSound,true,1.0f);
			if(mpTerrorSound)
			{
				mpTerrorSound->SetVolumeMul(0.0f);
				mlTerrorSoundID = mpTerrorSound->GetId();
			}
		}
	}
	if(mpTerrorSound && pSoundHandler->IsValid(mpTerrorSound, mlTerrorSoundID))
	{
		mpTerrorSound->SetVolumeMul(mfTerror);
		if(mfTerror <= 0)
		{
			mpTerrorSound->GetChannel()->Stop();
			mpTerrorSound = NULL;
			mlTerrorSoundID = -1;
		}
	}

	/////////////
	// See if there are any nearby enemies
	bool bBeingChased = mfTerror >= 0.95f;

	if(mbBeingChased == false && bBeingChased)
	{
		///////////
		// 33% chance to block escape from working
		mbBeingChased = true;
		StartRandomEscapeFail();
	}
	else if(mbBeingChased == true && bBeingChased == false)
	{
		//////////////
		// Remoive the block of escape
		mbBeingChased = false;
		EndRandomEscapeFail();
	}
}

//-----------------------------------------------------------------------

void cLuxPlayer::SpinHead(float afSpeed)
{
	// VR: spinning the view is a sure way to make people sick (vr_settings.cfg Comfort HeadSpin)
	if(cVRSystem::Get()->IsActive() && cLuxVRSettings::Get().mbHeadSpin==false) return;

	mvHeadSpinSpeed = cVector2f(cMath::RandRectf(-1,1), cMath::RandRectf(0,0.5f));
	if(mvHeadSpinSpeed.x == 0 && mvHeadSpinSpeed.y == 0) mvHeadSpinSpeed.x = 1;
	else mvHeadSpinSpeed.Normalize();

	mvHeadSpinSpeed *= afSpeed;

}

//-----------------------------------------------------------------------

void cLuxPlayer::UpdateHeadSpin(float afTimeStep)
{
	if(mvHeadSpinSpeed == 0)
	{
		return;
	}

	mpCamera->AddPitch(mvHeadSpinSpeed.y * afTimeStep);
	mpCamera->AddYaw(mvHeadSpinSpeed.x * afTimeStep);
	mpCharBody->SetYaw(mpCamera->GetYaw());

	float fLength = mvHeadSpinSpeed.Length();
	cVector2f vDir = mvHeadSpinSpeed;
	vDir.Normalize();

	fLength -= afTimeStep * mfHeadSpinDeacc;
	if(fLength < 0) fLength = 0;
    mvHeadSpinSpeed = vDir * fLength;
}

//-----------------------------------------------------------------------

void cLuxPlayer::UpdateFocusText(float afTimeStep)
{
	if(	gpBase->mpMessageHandler->IsPauseMessageActive() ||
		//gpBase->mpMessageHandler->IsMessageActive() || <- this interfeere with signs and stuff so not a good thing to have!
		msFocusText == _W(""))
	{
		mfFocusTextAlpha -= afTimeStep*2.0f;
		if(mfFocusTextAlpha < 0.0f) mfFocusTextAlpha =0;
	}
	else
	{
		mfFocusTextAlpha += afTimeStep*2.0f;
		if(mfFocusTextAlpha > 1.0f) mfFocusTextAlpha =1;
	}
}

//-----------------------------------------------------------------------

void cLuxPlayer::UpdateAvgSpeed(float afTimeStep)
{
	float fSpeed = mpCharBody->GetVelocity(afTimeStep).Length();
	mlstPrevSpeeds.push_back(fSpeed);
	if((int)mlstPrevSpeeds.size() > mlMaxPrevSpeeds) mlstPrevSpeeds.pop_front();

	mfAvgSpeed = 0;
	std::list<float>::iterator it = mlstPrevSpeeds.begin();
	for(; it != mlstPrevSpeeds.end(); ++it)
	{
		mfAvgSpeed += *it;
	}
	mfAvgSpeed /= (float)mlstPrevSpeeds.size();
}

//-----------------------------------------------------------------------

void cLuxPlayer::UpdateAvgMoveDir(float afTimeStep)
{
	mfAddMoveDirCount+=afTimeStep;
	if(mfAddMoveDirCount<0.25f) return;
	mfAddMoveDirCount =0;

	///////////////////////////////
	// Add new dir
	mlstPrevMoveDirs.push_back(mpCharBody->GetForward());
	if(mlstPrevMoveDirs.size()>12) mlstPrevMoveDirs.pop_front();

	////////////////////////////////////
	//Calculate the average walking direction
	tVector3fListIt firstIt = mlstPrevMoveDirs.begin();

	mvAvgMoveDir2D =0;
	float fTotalCount=0;
	for(; firstIt != mlstPrevMoveDirs.end(); ++firstIt)
	{
		cVector3f vDir = *firstIt;
		vDir.y=0; vDir.Normalize();

		mvAvgMoveDir2D += vDir;
		fTotalCount++;
	}
	mvAvgMoveDir2D = mvAvgMoveDir2D / fTotalCount;
	mvAvgMoveDir2D.Normalize();
}

//-----------------------------------------------------------------------

void cLuxPlayer::CreateCharacterBody(iPhysicsWorld *apPhysicsWorld)
{
	mpCharBody = apPhysicsWorld->CreateCharacterBody("Player",mvBodySize);

	//TODO: Use config here.
	mpCharBody->SetCustomGravity(	gpBase->mpGameCfg->GetVector3f("Player_Body","GravityForce",0) );
	mpCharBody->SetCustomGravityActive(true);
	mpCharBody->SetMass(mfDefaultMass);

	mpCharBody->SetAccurateClimbing(	gpBase->mpGameCfg->GetBool("Player_Body","AccurateClimbing",false) );
	mpCharBody->SetMaxNoSlideSlopeAngle(cMath::ToRad(gpBase->mpGameCfg->GetFloat("Player_Body","MaxNoSlideSlopeAngle",0) ) );
	mpCharBody->SetMaxPushMass(			gpBase->mpGameCfg->GetFloat("Player_Body","MaxPushMass",0) );
	mpCharBody->SetPushForce(			gpBase->mpGameCfg->GetFloat("Player_Body","PushForce",0) );
	mpCharBody->SetCharacterMaxPushMass(gpBase->mpGameCfg->GetFloat("Player_Body","CharacterMaxPushMass",0) );
	mpCharBody->SetCharacterPushForce(	gpBase->mpGameCfg->GetFloat("Player_Body","CharacterPushForce",0) );
	mpCharBody->SetMaxStepSize(			gpBase->mpGameCfg->GetFloat("Player_Body","MaxStepSize",0) );
	mpCharBody->SetMaxStepSizeInAir(	gpBase->mpGameCfg->GetFloat("Player_Body","MaxStepSizeInAir",0) );
	mpCharBody->SetStepClimbSpeed(		gpBase->mpGameCfg->GetFloat("Player_Body","StepClimbSpeed",0) );

	mpCharBody->SetCamera(mpCamera);
	mpCharBody->SetCameraPosAdd(mvCameraPosAdd);
	mpCharBody->SetCameraSmoothPosNum(	gpBase->mpGameCfg->GetInt("Player_Body","CameraSmoothPosNum",0) );

	//Create crouch extra
	mpCharBody->AddExtraSize(mvBodyCrouchSize);
	// VR: the thin body (Body CollisionWidth) for props, doors and enemies, the flat game's width for the level's
	// static geometry (Body WallWidth), so it can't squeeze into gaps the levels were built to keep closed
	if(cLuxVRSettings::Get().mbActive && cLuxVRSettings::Get().mfWallWidth > cLuxVRSettings::Get().mfBodyWidth)
		mpCharBody->SetStaticCollisionWidth(cLuxVRSettings::Get().mfWallWidth);

	//temp placement!
	mpCharBody->SetFeetPosition(cVector3f(8.5f, 5.2f, 9.25f));
	mpCharBody->Update(0.001f);

	//mpCharBody->SetActive(false);
}


//-----------------------------------------------------------------------

float cLuxPlayer::GetInfectionSpeedMul()
{
	switch ( GetInfectionLevel() )
	{
	case 1:
		{
			return mfInfectionLevelOneSpeedMul;
		}
	case 2:
		{
			return mfInfectionLevelTwoSpeedMul;
		}
	case 3:
		{
			return mfInfectionLevelThreeSpeedMul;
		}
	case 4:
		{
			return mfInfectionLevelFourSpeedMul;
		}

	}

	return 1.0f;
}

//-----------------------------------------------------------------------

bool cLuxPlayer::CanRun()
{
	return GetInfectionLevel() <= mnMaxInfectionLevelAtWhichPlayerCanRun;
}

//-----------------------------------------------------------------------

float cLuxPlayer::GetExhaustionFactor()
{
	return mpPlayerStamina ?  mpPlayerStamina->GetExhaustionFactor() : 0.0f;
}


//-----------------------------------------------------------------------

void cLuxPlayer::ReleasePlayerFromLimbo()
{
	GetHelperDeath()->ReleasePlayerFromLimbo();
}
//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// VR
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

/**
 * VR: the gameplay camera looks where the head looks, so aiming, grabbing, enemy sight and the listener follow the
 * head. The head yaw is fed into the camera/body yaw as it changes (like mouse look), the pitch comes straight from
 * the head and the head position becomes the camera's head offset. Turning (mouse, stick, snap, scripts) still
 * changes the camera/body yaw as before; the VR renderer takes the applied head yaw out again (cVRSystem).
 */
void cLuxPlayer::UpdateVRHead(float afTimeStep)
{
	cVRSystem *pVR = cVRSystem::Get();
	if(pVR->IsActive()==false) return;
	pVR->SetPlayerCamera(mpCamera);

	// Comfort vignette: speed of the last physics step (room-scale shifts move both positions, so they don't count)
	pVR->SetComfortMoveSpeed(mpCharBody->GetVelocity(afTimeStep).Length());

	////////////////////////
	// Yaw: apply the change since the last step (yaw limits of e.g. ladders may hold some of it back)
	float fConsumed = pVR->GetConsumedHeadYaw();
	float fDelta = cMath::GetAngleDistanceRad(fConsumed, pVR->GetHeadYaw());
	float fYawBefore = mpCamera->GetYaw();
	mpCamera->AddYaw(fDelta);
	float fApplied = mpCamera->GetYaw() - fYawBefore;
	mpCharBody->SetYaw(mpCamera->GetYaw());
	pVR->SetConsumedHeadYaw(fConsumed + fApplied);

	float fBodyYaw = mpCamera->GetYaw() - pVR->GetConsumedHeadYaw();

	////////////////////////
	// Pitch
	mpCamera->SetPitch(pVR->GetHeadPitch());

	////////////////////////
	// Body follows the head (room-scale), real crouch
	UpdateVRRoomScale(fBodyYaw);
	UpdateVRCrouch();

	////////////////////////
	// Position
	mpCamera->SetHeadOffset(cMath::MatrixMul(cMath::MatrixRotateY(fBodyYaw), pVR->GetPlayerHeadOffset()));
}

//-----------------------------------------------------------------------

/**
 * Room-scale: move the character body along with the head's horizontal movement (with collision), and move the
 * anchor (where the body is in tracking space) by what the body managed to move. Where a wall blocks the body,
 * the head can still move on (seated behaviour).
 */
void cLuxPlayer::UpdateVRRoomScale(float afBodyYaw)
{
	if(cLuxVRSettings::Get().mbRoomScale==false) return;
	if(mbActive==false || mMoveState != eLuxMoveState_Normal) return;
	if(mState != eLuxPlayerState_Normal && mState != eLuxPlayerState_HandObject && mState != eLuxPlayerState_UseItem) return;

	cVRSystem *pVR = cVRSystem::Get();
	cVector3f vHead = pVR->GetHeadPosition();
	cVector3f vAnchor = pVR->GetHeadAnchor();
	cVector3f vLocal(vHead.x - vAnchor.x, 0, vHead.z - vAnchor.z);
	float fLength = vLocal.Length();
	if(fLength < 0.01f) return;
	if(fLength > 0.1f) vLocal = vLocal * (0.1f / fLength); // per step, the rest follows next step

	cVector3f vMove = cMath::MatrixMul(cMath::MatrixRotateY(afBodyYaw), vLocal);
	cVector3f vPos = mpCharBody->GetPosition();
	cVector3f vLift(0, 0.01f, 0);

	// Whole move, else slide along one axis
	cVector3f vTry[3] = { vMove, cVector3f(vMove.x,0,0), cVector3f(0,0,vMove.z) };
	for(int i=0; i<3; ++i)
	{
		if(vTry[i].SqrLength() < 0.000001f) continue;
		if(mpCharBody->CheckCharacterFits(vPos + vTry[i] + vLift))
		{
			mpCharBody->ShiftPosition(vTry[i]); // SetPosition would reset velocity and camera smoothing (stutter)
			cVector3f vLocalMoved = cMath::MatrixMul(cMath::MatrixRotateY(-afBodyYaw), vTry[i]);
			pVR->SetHeadAnchor(cVector3f(vAnchor.x + vLocalMoved.x, 0, vAnchor.z + vLocalMoved.z));
			return;
		}
	}
}

//-----------------------------------------------------------------------

/**
 * VR: remembers the body's last 64 steps and writes them to the log when a fall over ~2 m starts (and when you
 * leave the map), so it shows how you got somewhere you shouldn't be.
 */
void cLuxPlayer::UpdateVRBodyTrail()
{
	if(cLuxVRSettings::Get().mbActive==false) return;

	cLuxVRBodyTrailStep &step = gvVRBodyTrail[glVRBodyTrailCount++ % 64];
	step.mvPos = mpCharBody->GetFeetPosition();
	step.mvVel = mpCharBody->GetForceVelocity();
	step.mbOnGround = mpCharBody->IsOnGround();

	if(step.mbOnGround) gbVRFallLogged = false;
	else if(gbVRFallLogged==false && step.mvVel.y < -6.0f)
	{
		gbVRFallLogged = true;
		Log("VR: falling at (%.2f %.2f %.2f), the last second:\n", step.mvPos.x, step.mvPos.y, step.mvPos.z);
		LogVRBodyTrail(4);
	}
}

//-----------------------------------------------------------------------

/**
 * Crouching in real life (head CrouchDepth below the recentered height) crouches the character. While the game
 * lowers the camera to its crouch height, the real head drop is taken back out, so the view ends at the game's
 * crouch height (and goes lower if you crouch deeper).
 */
void cLuxPlayer::UpdateVRCrouch()
{
	cVRSystem *pVR = cVRSystem::Get();
	const cLuxVRSettings &settings = cLuxVRSettings::Get();
	cLuxMoveState_Normal *pNormal = static_cast<cLuxMoveState_Normal*>(mvMoveStates[eLuxMoveState_Normal]);

	if(settings.mbRealCrouch && mbActive && mMoveState == eLuxMoveState_Normal)
	{
		float fHeadY = pVR->GetHeadPosition().y;
		if(pNormal->IsCrouching()==false)
		{
			if(fHeadY < -settings.mfCrouchDepth && mbCrouchDisabled==false && mpInfectionCollapse->IsActive()==false)
			{
				pNormal->SetCrouch(true);
				mbVRRealCrouch = pNormal->IsCrouching();
			}
		}
		else if(mbVRRealCrouch && fHeadY > -settings.mfCrouchDepth + 0.05f)
		{
			pNormal->SetCrouch(false); // stays crouched if there is no room to stand
		}
	}
	if(pNormal->IsCrouching()==false) mbVRRealCrouch = false;

	////////////////////////
	// Height compensation, following the game's own (smooth) crouch movement: from a real crouch until the game has
	// fully stood up again (so standing up in real life gives no extra height change). A crouch with the button is
	// not compensated.
	float fCrouchDrop = mvBodySize.y - mvBodyCrouchSize.y;
	float fGameDrop = cMath::Max(-GetHeadPosAdd(eLuxHeadPosAdd_Main).y, 0.0f);
	if(mbVRRealCrouch) mbVRCrouchComp = true;
	else if(pNormal->IsCrouching()) mbVRCrouchComp = false;			// crouched with the button
	else if(fGameDrop < 0.005f) mbVRCrouchComp = false;			// fully stood up

	float fYAdd = 0;
	if(mbVRCrouchComp)
	{
		float fT = fCrouchDrop > 0 ? cMath::Clamp(fGameDrop / fCrouchDrop, 0.0f, 1.0f) : 1.0f;
		// Keep height: take the game's whole crouch drop back out (the view stays at your real head), else only
		// the real drop (the view ends at the game's crouch height)
		fYAdd = (settings.mbRealCrouchKeepHeight ? fCrouchDrop : settings.mfCrouchDepth) * fT;
	}
	pVR->SetHeadYAdd(fYAdd);
}

//-----------------------------------------------------------------------

void cLuxPlayer::VRSnapTurn(float afAngle)
{
	if(mbActive==false) return;
	// Holding a prop (InteractGrab) may turn too: the prop follows the hand. Doors/levers/valves/sliders may not (the
	// hand jumps along with the turn and would yank them).
	if(mState != eLuxPlayerState_Normal && mState != eLuxPlayerState_HandObject && mState != eLuxPlayerState_UseItem &&
	   mState != eLuxPlayerState_InteractGrab) return;

	mpCamera->AddYaw(afAngle);
	mpCharBody->SetYaw(mpCamera->GetYaw());

	// The hand's jump is not a throw
	if(mState == eLuxPlayerState_InteractGrab)
		static_cast<cLuxPlayerState_InteractGrab*>(mvStates[eLuxPlayerState_InteractGrab])->VRResetHandVelocity();
	if(mpVRSecondGrab) mpVRSecondGrab->VRResetHandVelocity();
}

//-----------------------------------------------------------------------

/**
 * VR: the pointing hand holds a picked-up object and the other hand wants to interact. The grab moves out of the
 * state machine into the second-grab slot (it keeps following the hand that grabbed it) and the player is back in the
 * Normal state, so the other hand can grab or use something too. False if there is nothing to hand over.
 */
bool cLuxPlayer::VRHandOverGrab()
{
	if(mState != eLuxPlayerState_InteractGrab || mpVRSecondGrab) return false;
	cLuxPlayerState_InteractGrab *pGrab = static_cast<cLuxPlayerState_InteractGrab*>(mvStates[eLuxPlayerState_InteractGrab]);
	if(pGrab->IsHeldByVRHand()==false) return false;

	mpVRSecondGrab = pGrab;
	mpVRSecondGrab->SetVRSecondHand(true);
	mvStates[eLuxPlayerState_InteractGrab] = mpVRSpareGrab;
	mpVRSpareGrab = NULL;

	mState = eLuxPlayerState_Normal;
	mvStates[mState]->SetPreviousState(eLuxPlayerState_InteractGrab);
	mvStates[mState]->OnEnterState(eLuxPlayerState_InteractGrab);
	return true;
}

//-----------------------------------------------------------------------

/** VR: lets go of what the second hand holds (restores the object's physics). */
void cLuxPlayer::ReleaseVRSecondGrab()
{
	if(mpVRSecondGrab==NULL) return;

	// Leaving a grab resets the player's mass and speed; keep them if the pointing hand still holds something
	bool bKeep = mState == eLuxPlayerState_InteractGrab && GetCharacterBody();
	float fMass = bKeep ? GetCharacterBody()->GetMass() : 0;
	float fSpeedMul = GetInteractionMoveSpeedMul();

	mpVRSecondGrab->OnLeaveState(eLuxPlayerState_Normal);
	mpVRSecondGrab->SetVRSecondHand(false);
	mpVRSpareGrab = mpVRSecondGrab;
	mpVRSecondGrab = NULL;

	if(bKeep)
	{
		GetCharacterBody()->SetMass(fMass);
		SetInteractionMoveSpeedMul(fSpeedMul);
	}
}

//-----------------------------------------------------------------------

void cLuxPlayer::VRSecondGrabOnDestroyEntity(iLuxEntity *apEntity)
{
	if(mpVRSecondGrab==NULL) return;
	mpVRSecondGrab->OnDestroyEntity(apEntity);
	if(mpVRSecondGrab->IsVRSecondHandDone()) ReleaseVRSecondGrab();
}

void cLuxPlayer::VRSecondGrabOnAttachBodyToStickyArea(iPhysicsBody *apBody)
{
	if(mpVRSecondGrab==NULL) return;
	mpVRSecondGrab->OnAttachBodyToStickyArea(apBody);
	if(mpVRSecondGrab->IsVRSecondHandDone()) ReleaseVRSecondGrab();
}

//-----------------------------------------------------------------------

/** VR: the second hand holds its object until its grip is let go (a throw if the hand moves). */
void cLuxPlayer::UpdateVRSecondGrab(float afTimeStep)
{
	if(mpVRSecondGrab==NULL) return;
	cVRSystem *pVR = cVRSystem::Get();
	if(IsDead() || pVR->IsActive()==false)
	{
		ReleaseVRSecondGrab();
		return;
	}

	if(pVR->GetHandGrip(mpVRSecondGrab->GetVRHand())==false)
		mpVRSecondGrab->OnDoAction(eLuxPlayerAction_Interact, false);
	if(mpVRSecondGrab->IsVRSecondHandDone()==false)
		mpVRSecondGrab->PostUpdate(afTimeStep);
	if(mpVRSecondGrab->IsVRSecondHandDone())
		ReleaseVRSecondGrab();
}

//-----------------------------------------------------------------------
