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

#include "LuxStaticProp.h"
#include "LuxProp_Object.h"
#include "LuxVR.h"

#include "LuxMap.h"
#include "LuxPlayer.h"
#include "LuxInteractConnections.h"



//////////////////////////////////////////////////////////////////////////
// LOADER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxStaticPropLoader::cLuxStaticPropLoader(const tString& asName) : cEntityLoader_Object(asName)
{
	mbLoadAsStatic = true;
	mbCreatesStaticEntity = true;
	mpVRGrabLoader = NULL;
}

cLuxStaticPropLoader::~cLuxStaticPropLoader()
{
	if(mpVRGrabLoader) hplDelete(mpVRGrabLoader);
}

//-----------------------------------------------------------------------

/**
 * VR: small decoration props (glasses, books, toys...) are static in the flat game. With Hands GrabSmallProps they are
 * loaded as grab objects instead; they stay fixed (mass 0) until picked up.
 */
iEntity3D* cLuxStaticPropLoader::Load(const tString &asName, int alID, bool abActive, cXmlElement *apRootElem,
									const cMatrixf &a_mtxTransform, const cVector3f &avScale,
									cWorld *apWorld, const tString &asFileName, const tWString &asFullPath, cResourceVarsObject *apInstanceVars)
{
	if(LuxVRSmallPropMass(apRootElem, avScale, asFileName) > 0)
	{
		if(mpVRGrabLoader==NULL) mpVRGrabLoader = hplNew(cLuxPropLoader_Object, (GetName()));
		mpVRGrabLoader->SetVRFromStaticProp(true);
		iEntity3D *pEntity = mpVRGrabLoader->Load(asName, alID, abActive, apRootElem, a_mtxTransform, avScale, apWorld, asFileName, asFullPath, apInstanceVars);
		mpVRGrabLoader->SetVRFromStaticProp(false);
		return pEntity;
	}
	return cEntityLoader_Object::Load(asName, alID, abActive, apRootElem, a_mtxTransform, avScale, apWorld, asFileName, asFullPath, apInstanceVars);
}

//-----------------------------------------------------------------------

//-----------------------------------------------------------------------

void cLuxStaticPropLoader::BeforeLoad(cXmlElement *apRootElem, const cMatrixf &a_mtxTransform,cWorld *apWorld, cResourceVarsObject *apInstanceVars)
{

}

//-----------------------------------------------------------------------

void cLuxStaticPropLoader::AfterLoad(cXmlElement *apRootElem, const cMatrixf &a_mtxTransform,cWorld *apWorld, cResourceVarsObject *apInstanceVars)
{
	cLuxMap *pMap = gpBase->mpCurrentMapLoading;
	if(pMap==NULL) return;

	// ATTENTION!
	// The entity classes is only be used if for some reason 
	// static props needs to be turned of or what not.

	/*cLuxStaticProp *pStaticProp = hplNew( cLuxStaticProp, (msName, mlID) );

	//////////////////////////////
	// Set data
	pStaticProp->mpMap = pMap;
	pStaticProp->msFileName = msFileName;
	pStaticProp->m_mtxOnLoadTransform = a_mtxTransform;

	pStaticProp->mvBodies = mvBodies;
	pStaticProp->mvJoints = mvJoints;

	pStaticProp->mvLights = mvLights;
	pStaticProp->mvParticleSystems = mvParticleSystems;
	pStaticProp->mvBillboards = mvBillboards;
	pStaticProp->mvBeams = mvBeams;
	pStaticProp->mvSoundEntities = mvSoundEntities;

	pStaticProp->mpMeshEntity = mpEntity;


	//////////////////////////////
	//Add to the current map
	pMap->AddStaticProp(pStaticProp);*/
	
	///////////////////////////////
	// Instance vars
	if(apInstanceVars)
	{
		if(mpEntity)
		{
			mpEntity->SetRenderFlagBit(eRenderableFlag_ShadowCaster, apInstanceVars->GetVarBool("CastShadows", true));
		}
	}
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTOR
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------


cLuxStaticProp::cLuxStaticProp(const tString &asName, int alID)
{
	msName = asName;
	mlID = alID;
}

cLuxStaticProp::~cLuxStaticProp()
{
	cWorld *pWorld = mpMap->GetWorld();
	iPhysicsWorld *pPhysicsWorld = pWorld->GetPhysicsWorld();

	////////////////////
	// Destroy physics
	{
		//Joints
		for(size_t i=0; i<mvJoints.size(); ++i)
		{
			iPhysicsJoint *pJoint = mvJoints[i];

			if(pJoint && pPhysicsWorld->JointExists(pJoint))
			{
				pPhysicsWorld->DestroyJoint(pJoint);
			}
		}

		//Bodies
		for(size_t i=0; i<mvBodies.size(); ++i)
		{
			iPhysicsBody *pBody = mvBodies[i];

			pWorld->GetPhysicsWorld()->DestroyBody(pBody);
		}
	}

	////////////////////
	// Destroy graphics
	{
		//Mesh entity
		if(mpMeshEntity) pWorld->DestroyMeshEntity(mpMeshEntity);

		//Lights
		for(size_t i=0; i<mvLights.size(); ++i) pWorld->DestroyLight(mvLights[i]);

		//Particle systems
		for(size_t i=0; i<mvParticleSystems.size(); ++i) 
		{
			cParticleSystem *pPS = mvParticleSystems[i];
			if(pPS && pWorld->ParticleSystemExists(pPS)) pPS->Kill();
		}

		//Billboards
		for(size_t i=0; i<mvBillboards.size(); ++i)	pWorld->DestroyBillboard(mvBillboards[i]);

		//Beams
		for(size_t i=0; i<mvBeams.size(); ++i)		pWorld->DestroyBeam(mvBeams[i]);

		//Sound entities
		for(size_t i=0; i<mvSoundEntities.size(); ++i) 	pWorld->DestroySoundEntity(mvSoundEntities[i]);
	}	
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SAVE DATA STUFF
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

kBeginSerializeBase(iLuxStaticProp_SaveData)

kSerializeVar(msName, eSerializeType_String)
kSerializeVar(mlID, eSerializeType_Int32)

kSerializeVar(msFileName, eSerializeType_String)
kSerializeVar(m_mtxOnLoadTransform, eSerializeType_Matrixf)

kEndSerialize()

//-----------------------------------------------------------------------
