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

#ifndef LUX_VR_H
#define LUX_VR_H

#include "vr/VRSystem.h"
#include "resources/ConfigFile.h"

//----------------------------------------------

/** Game-side VR settings (Locomotion, Body and Comfort sections of vr_settings.cfg). */
/** Default Hands GrabSmallPropsExclude: wall-mounted things and invisible blockers stay fixed. */
const char * const kLuxVRGrabSmallPropsExclude = "invisible mount sign wall hook pulley frame chain";

class cLuxVRSettings
{
public:
	enum eTurn { eTurn_Snap, eTurn_Smooth, eTurn_Off };
	enum eLookAt { eLookAt_Snap, eLookAt_Smooth, eLookAt_Off };

	bool mbActive;

	eTurn mStickTurn;
	eTurn mMouseTurn;
	float mfSnapAngle;		// radians
	bool mbRoomScale;
	float mfStickStraightAngle;	// radians: walk stick pushes this close to an axis go straight

	bool mbRealCrouch;
	float mfCrouchDepth;	// metres below the recentered head height
	bool mbShowArm;			// the flat game's arm model holding the lantern

	bool mbHeadSpin;
	bool mbHeadBob;
	float mfScreenShake;	// 0..1 of the flat game's shake
	eLookAt mLookAt;
	bool mbRadialBlur;
	bool mbImageTrail;

	bool mbLanternInHand;	// controllers: the lantern hangs from the lantern hand
	bool mbLanternUpright;	// it hangs straight down (only the hand's yaw), else it follows the hand's tilt
	enum eLanternTurn { eLanternTurn_Hand, eLanternTurn_Point, eLanternTurn_Arm, eLanternTurn_Fixed };
	float mfLanternArmPivot;	// metres behind the head that LanternTurn=Arm measures the hand's direction from
	eLanternTurn mLanternTurn;	// what turns an upright lantern: the hand, where the controller points, the arm's direction, or nothing
	float mfLanternYaw;		// radians, extra turn of the lantern around the vertical, counterclockwise seen from above
	cVector3f mvLanternOffset;	// metres, in the hand's frame (x right, y up, z back)
	bool mbHaptics;
	bool mbShowControllers;	// SteamVR's controller models at the hands
	bool mbOffHandInteract;	// with the lantern put away the lantern hand can grab and use things too
	bool mbLanternHandInteract;	// ...and also while it holds the lantern
	bool mbBothHandsHold;	// one hand keeps holding a picked-up object while the other hand grabs or uses something
	float mfGrabSmallProps;	// metres: decoration props (static in the flat game) up to this size can be picked up; 0 = off
	tStringVec mvGrabSmallPropsExclude;	// ...except entity files whose name contains one of these (lower case)
	bool mbShowHands;		// hand models at the controllers (fingers follow the Index finger tracking)
	cVector3f mvHandOffset;	// metres: right hand model's wrist in the controller's pointing frame (x right, y up, z back); left is mirrored
	cVector3f mvHandRotation;	// radians (x, y, z, applied z first): right hand model's turn in that frame; left is mirrored
	float mfFingerCurl;		// degrees a finger joint bends when fully curled (thumb uses 60% of it)
	float mfThrowMul;		// hand speed -> thrown object speed when letting go of a held object
	float mfThrowMaxSpeed;	// m/s
	float mfInteractStrength;	// hand metres -> mouse movement for doors, levers, wheels, sliders
	float mfGrabReach;		// metres: objects are only picked up this close to the controller tip
	bool mbLanternHip;		// grip with the lantern hand at the hip takes the lantern out / puts it away
	cVector3f mvLanternHipOffset;	// hip relative to the head: x outward (to the lantern hand's side), y up, z back
	float mfLanternHipRadius;
	float mfInteractReach;	// metres: doors, levers, wheels, drawers only react this close to the controller tip
	float mfBodyWidth;		// metres: width of the player's collision body in VR (flat game: 0.8)
	bool mbOutOfMapKill;	// you die when pushed/fallen out of the map (2 m below or 5 m beside its static geometry)
	float mfEnemyAimAssist;	// radians: an enemy attack is aimed at you when you're within this angle of where it faces (0 = off)
	float mfEnemyHitWidth;	// metres: width of the player when enemy attacks test for a hit (flat game: the body, 0.8)
	bool mbRealCrouchKeepHeight;	// real crouch: stealth crouch, but the view stays at your real head height

	float mfReticleSize;	// width in metres of the 800-unit crosshair area at 1 m distance
	float mfReticleMinDist, mfReticleMaxDist;

	static const cLuxVRSettings& Get()
	{
		static cLuxVRSettings gSettings;
		static bool bLoaded = false;
		if(bLoaded==false)
		{
			gSettings.Load();
			bLoaded = true;
		}
		return gSettings;
	}

private:
	static eTurn ToTurn(const tString& asX, eTurn aDefault)
	{
		tString s = cString::ToLowerCase(asX);
		if(s=="snap") return eTurn_Snap;
		if(s=="smooth") return eTurn_Smooth;
		if(s=="off") return eTurn_Off;
		return aDefault;
	}

	void Load()
	{
		hpl::cVRSystem *pVR = hpl::cVRSystem::Get();
		mbActive = pVR->IsActive();
		cConfigFile *pCfg = pVR->GetConfig();

		mStickTurn = eTurn_Snap; mMouseTurn = eTurn_Smooth;
		mfSnapAngle = cMath::ToRad(30.0f);
		mbRoomScale = true;
		mfStickStraightAngle = cMath::ToRad(20.0f);
		mbRealCrouch = true; mfCrouchDepth = 0.35f; mbShowArm = false;
		mbHeadSpin = false; mLookAt = eLookAt_Snap;
		mbHeadBob = false; mfScreenShake = 0.3f;
		mbRadialBlur = false; mbImageTrail = false;
		mbLanternInHand = true; mbLanternUpright = true; mLanternTurn = eLanternTurn_Point; mfLanternArmPivot = 0.5f; mvLanternOffset = 0; mfLanternYaw = cMath::ToRad(90.0f); mbHaptics = true; mbShowControllers = false; mbShowHands = true; mbOffHandInteract = true; mbLanternHandInteract = true; mbBothHandsHold = true; mfGrabSmallProps = 0.5f; mvGrabSmallPropsExclude.clear(); cString::GetStringVec(kLuxVRGrabSmallPropsExclude, mvGrabSmallPropsExclude); mvHandOffset = cVector3f(0.03f, -0.04f, 0.14f); mvHandRotation = 0; mfFingerCurl = 85.0f; mfThrowMul = 1.0f; mfThrowMaxSpeed = 10.0f; mfInteractStrength = 1.0f; mfGrabReach = 0.15f; mbLanternHip = true; mvLanternHipOffset = cVector3f(0.2f, -0.7f, 0.15f); mfLanternHipRadius = 0.25f; mfInteractReach = 0.2f; mfBodyWidth = 0.3f; mfEnemyHitWidth = 0.8f; mfEnemyAimAssist = cMath::ToRad(90.0f); mbOutOfMapKill = true; mbRealCrouchKeepHeight = true;
		mfReticleSize = 0.6f; mfReticleMinDist = 0.3f; mfReticleMaxDist = 8.0f;
		if(pCfg==NULL) return;

		mStickTurn = ToTurn(pCfg->GetString("Locomotion", "StickTurn", "Snap"), eTurn_Snap);
		mMouseTurn = ToTurn(pCfg->GetString("Locomotion", "MouseTurn", "Smooth"), eTurn_Smooth);
		mfSnapAngle = cMath::ToRad(cMath::Clamp(pCfg->GetFloat("Locomotion", "SnapAngle", 30.0f), 5.0f, 180.0f));
		mbRoomScale = pCfg->GetBool("Locomotion", "RoomScale", true);
		mfStickStraightAngle = cMath::ToRad(cMath::Clamp(pCfg->GetFloat("Locomotion", "StraightAngle", 20.0f), 0.0f, 44.0f));

		mbRealCrouch = pCfg->GetBool("Body", "RealCrouch", true);
		mfCrouchDepth = cMath::Clamp(pCfg->GetFloat("Body", "CrouchDepth", 0.35f), 0.1f, 1.0f);
		mbShowArm = pCfg->GetBool("Body", "ShowArm", false);

		mbHeadSpin = pCfg->GetBool("Comfort", "HeadSpin", false);
		mbHeadBob = pCfg->GetBool("Comfort", "HeadBob", false);
		mfScreenShake = cMath::Clamp(pCfg->GetFloat("Comfort", "ScreenShake", 0.3f), 0.0f, 1.0f);
		tString sLookAt = cString::ToLowerCase(pCfg->GetString("Comfort", "LookAt", "Snap"));
		mLookAt = sLookAt=="smooth" ? eLookAt_Smooth : (sLookAt=="off" ? eLookAt_Off : eLookAt_Snap);
		mbRadialBlur = pCfg->GetBool("Comfort", "RadialBlur", false);
		mbImageTrail = pCfg->GetBool("Comfort", "ImageTrail", false);

		mbLanternInHand = pCfg->GetBool("Hands", "LanternInHand", true);
		mbLanternUpright = pCfg->GetBool("Hands", "LanternUpright", true);
		tString sLanternTurn = cString::ToLowerCase(pCfg->GetString("Hands", "LanternTurn", "Point"));
		mfLanternArmPivot = cMath::Clamp(pCfg->GetFloat("Hands", "LanternArmPivot", 0.5f), 0.0f, 3.0f);
		mLanternTurn = sLanternTurn=="hand" ? eLanternTurn_Hand : (sLanternTurn=="fixed" ? eLanternTurn_Fixed : (sLanternTurn=="arm" ? eLanternTurn_Arm : eLanternTurn_Point));
		mvLanternOffset = pCfg->GetVector3f("Hands", "LanternOffset", 0);
		mfLanternYaw = cMath::ToRad(pCfg->GetFloat("Hands", "LanternYaw", 90.0f));
		mbHaptics = pCfg->GetBool("Hands", "Haptics", true);
		mbShowHands = pCfg->GetBool("Hands", "ShowHands", true);
		mbOffHandInteract = pCfg->GetBool("Hands", "OffHandInteract", true);
		mbLanternHandInteract = pCfg->GetBool("Hands", "LanternHandInteract", true);
		mbBothHandsHold = pCfg->GetBool("Hands", "BothHandsHold", true);
		mfGrabSmallProps = pCfg->GetBool("Hands", "GrabSmallProps", true) ? cMath::Clamp(pCfg->GetFloat("Hands", "GrabSmallPropsMaxSize", 0.5f), 0.05f, 2.0f) : 0.0f;
		mvGrabSmallPropsExclude.clear();
		cString::GetStringVec(cString::ToLowerCase(pCfg->GetString("Hands", "GrabSmallPropsExclude", kLuxVRGrabSmallPropsExclude)), mvGrabSmallPropsExclude);
		mbShowControllers = pCfg->GetBool("Hands", "ShowControllers", !mbShowHands);
		mvHandOffset = pCfg->GetVector3f("Hands", "HandOffset", mvHandOffset);
		cVector3f vHandRotDeg = pCfg->GetVector3f("Hands", "HandRotation", 0);
		mvHandRotation = cVector3f(cMath::ToRad(vHandRotDeg.x), cMath::ToRad(vHandRotDeg.y), cMath::ToRad(vHandRotDeg.z));
		mfFingerCurl = cMath::Clamp(pCfg->GetFloat("Hands", "FingerCurl", 85.0f), 0.0f, 120.0f);
		mfThrowMul = cMath::Clamp(pCfg->GetFloat("Hands", "ThrowStrength", 1.0f), 0.0f, 3.0f);
		mfThrowMaxSpeed = cMath::Clamp(pCfg->GetFloat("Hands", "ThrowMaxSpeed", 10.0f), 1.0f, 30.0f);
		mfInteractStrength = cMath::Clamp(pCfg->GetFloat("Hands", "InteractStrength", 1.0f), 0.1f, 20.0f);
		mfGrabReach = cMath::Clamp(pCfg->GetFloat("Hands", "GrabReach", 0.15f), 0.0f, 3.0f);
		mbLanternHip = pCfg->GetBool("Hands", "LanternHip", true);
		mvLanternHipOffset = pCfg->GetVector3f("Hands", "LanternHipOffset", cVector3f(0.2f, -0.7f, 0.15f));
		mfLanternHipRadius = cMath::Clamp(pCfg->GetFloat("Hands", "LanternHipRadius", 0.25f), 0.05f, 1.0f);
		mfInteractReach = cMath::Clamp(pCfg->GetFloat("Hands", "InteractReach", 0.2f), 0.0f, 3.0f);
		mfBodyWidth = cMath::Clamp(pCfg->GetFloat("Body", "CollisionWidth", 0.3f), 0.2f, 0.8f);
		mbOutOfMapKill = pCfg->GetBool("Body", "OutOfMapKill", true);
		mfEnemyAimAssist = cMath::ToRad(cMath::Clamp(pCfg->GetFloat("Body", "EnemyAimAssist", 90.0f), 0.0f, 180.0f));
		mfEnemyHitWidth = cMath::Clamp(pCfg->GetFloat("Body", "EnemyHitWidth", 0.8f), 0.2f, 1.5f);
		mbRealCrouchKeepHeight = pCfg->GetBool("Body", "RealCrouchKeepsHeight", true);

		mfReticleSize = 0.6f * cMath::Clamp(pCfg->GetFloat("Reticle", "Size", 1.0f), 0.2f, 5.0f);
		mfReticleMaxDist = cMath::Clamp(pCfg->GetFloat("Reticle", "MaxDistance", 8.0f), 1.0f, 50.0f);

		Log(" VR game settings: stick turn %d, mouse turn %d (0 snap, 1 smooth, 2 off), snap %.0f deg, room-scale %d, "
			"real crouch %d (%.2f m), head spin %d, head bob %d, shake %.2f, look-at %d, radial blur %d, image trail %d\n",
			mStickTurn, mMouseTurn, cMath::ToDeg(mfSnapAngle), mbRoomScale, mbRealCrouch, mfCrouchDepth,
			mbHeadSpin, mbHeadBob, mfScreenShake, mLookAt, mbRadialBlur, mbImageTrail);
	}
};

//----------------------------------------------

/**
 * VR pointer ray in the world: from the pointing hand's controller (Input Pointer="Hand", the controller is tracked),
 * for the player camera's current logic step. False if the head (camera) should aim instead.
 */
/** The VR pointer ray starts this far behind the controller tip (focus distances include it). */
const float kfLuxVRRayBack = 0.1f;

/** World matrix of a hand's aim pose (controller tip, -Z forward). False if VR or that hand is not tracked. */
inline bool LuxVRGetHandMatrix(cCamera *apCamera, hpl::eVRHand aHand, cMatrixf &a_mtxWorld)
{
	hpl::cVRSystem *pVR = hpl::cVRSystem::Get();
	if(pVR->IsActive()==false || pVR->HasControllerInput()==false) return false;
	cMatrixf mtxAim;
	if(pVR->GetHandAimPose(aHand, mtxAim)==false) return false;
	a_mtxWorld = cMath::MatrixMul(pVR->GetSeatedToWorld(apCamera), mtxAim);
	return true;
}

inline bool LuxVRGetPointerMatrix(cCamera *apCamera, cMatrixf &a_mtxWorld)
{
	hpl::cVRSystem *pVR = hpl::cVRSystem::Get();
	if(pVR->IsActive()==false || pVR->UsesHandPointer()==false) return false;
	return LuxVRGetHandMatrix(apCamera, pVR->GetPointerHand(), a_mtxWorld);
}

inline bool LuxVRGetPointerRay(cCamera *apCamera, cVector3f &avStart, cVector3f &avDir)
{
	hpl::cVRSystem *pVR = hpl::cVRSystem::Get();
	if(pVR->IsActive()==false || pVR->UsesHandPointer()==false) return false;
	cMatrixf mtxAim;
	if(pVR->GetHandAimPose(pVR->GetPointerHand(), mtxAim)==false) return false;
	cMatrixf mtxWorld = cMath::MatrixMul(pVR->GetSeatedToWorld(apCamera), mtxAim);
	avDir = cVector3f(-mtxWorld.m[0][2], -mtxWorld.m[1][2], -mtxWorld.m[2][2]);
	avDir.Normalize();
	// Start a bit behind the tip, so an object the controller is inside of is still hit (see kfLuxVRRayBack)
	avStart = mtxWorld.GetTranslation() - avDir * kfLuxVRRayBack;
	return true;
}

/** Where an interaction with the hand comes from (set by the input handler around DoAction(Interact)). */
enum eLuxVRInteractSource { eLuxVRInteractSource_None, eLuxVRInteractSource_Trigger, eLuxVRInteractSource_Grip };
inline eLuxVRInteractSource& LuxVRInteractSource(){ static eLuxVRInteractSource gSource = eLuxVRInteractSource_None; return gSource; }

//----------------------------------------------

#endif // LUX_VR_H
