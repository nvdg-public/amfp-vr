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

#include "impl/MeshLoaderFBXCache.h"
#include "impl/MeshLoaderMSH.h"

#include "system/LowLevelSystem.h"
#include "system/String.h"
#include "system/Platform.h"

#include "graphics/Mesh.h"
#include "graphics/Animation.h"

#include <stdlib.h>
#ifdef WIN32
#include <direct.h>
#endif

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	static tWString NormalizePath(const tWString& asPath)
	{
		tWString sPath = cString::ReplaceCharToW(asPath, _W("\\"), _W("/"));
		if(sPath.size() > 0 && sPath[sPath.size()-1] != _W('/')) sPath += _W("/");
		return sPath;
	}

	//-----------------------------------------------------------------------

	cMeshLoaderFBXCache::cMeshLoaderFBXCache(iLowLevelGraphics *apLowLevelGraphics, cMeshLoaderMSH *apMeshLoaderMSH) : iMeshLoader(apLowLevelGraphics)
	{
		mpMeshLoaderMSH = apMeshLoaderMSH;
		AddSupportedExtension("fbx");

#ifdef WIN32
		const wchar_t *pOverlay = _wgetenv(L"HPL_CACHE_OVERLAY");
		if(pOverlay && pOverlay[0]) msOverlayDir = NormalizePath(pOverlay);

		wchar_t sCwd[1024];
		if(_wgetcwd(sCwd, 1024)) msWorkingDir = NormalizePath(sCwd);
#else
		const char *pOverlay = getenv("HPL_CACHE_OVERLAY");
		if(pOverlay && pOverlay[0]) msOverlayDir = NormalizePath(cString::To16Char(pOverlay));
#endif
		if(msOverlayDir != _W("")) Log(" FBX cache overlay: '%s'\n", cString::To8Char(msOverlayDir).c_str());
	}

	//-----------------------------------------------------------------------

	cMeshLoaderFBXCache::~cMeshLoaderFBXCache()
	{
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cMesh* cMeshLoaderFBXCache::LoadMesh(const tWString& asFile, tMeshLoadFlag aFlags)
	{
		tWString sCache = GetCacheFile(asFile, _W("msh"));
		if(sCache == _W(""))
		{
			Error("No .msh cache for '%s' (FBX loading is not available in this build)\n", cString::To8Char(asFile).c_str());
			return NULL;
		}

		cMesh *pMesh = mpMeshLoaderMSH->LoadMesh(sCache, aFlags);
		if(pMesh) pMesh->SetFullPath(asFile); //Use the fbx as full path, as cMeshLoaderFBX does
		return pMesh;
	}

	//-----------------------------------------------------------------------

	cAnimation* cMeshLoaderFBXCache::LoadAnimation(const tWString& asFile)
	{
		tWString sCache = GetCacheFile(asFile, _W("anm"));
		if(sCache == _W(""))
		{
			Error("No .anm cache for '%s' (FBX loading is not available in this build)\n", cString::To8Char(asFile).c_str());
			return NULL;
		}

		cAnimation *pAnim = mpMeshLoaderMSH->LoadAnimation(sCache);
		if(pAnim) pAnim->SetFullPath(asFile);
		return pAnim;
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	tWString cMeshLoaderFBXCache::GetCacheFile(const tWString& asFile, const tWString& asExt)
	{
		tWString sCache = cString::SetFileExtW(asFile, asExt);

		////////////////////////////////
		// Overlay: same path relative to the working directory (the game folder)
		if(msOverlayDir != _W(""))
		{
			tWString sRel = cString::ReplaceCharToW(sCache, _W("\\"), _W("/"));
			if(msWorkingDir != _W("") && cString::ToLowerCaseW(sRel).find(cString::ToLowerCaseW(msWorkingDir)) == 0)
				sRel = sRel.substr(msWorkingDir.size());
			while(sRel.size() >= 2 && sRel.substr(0,2) == _W("./")) sRel = sRel.substr(2);
			while(sRel.size() >= 1 && sRel[0] == _W('/')) sRel = sRel.substr(1);

			tWString sOverlayFile = msOverlayDir + sRel;
			if(cPlatform::FileExists(sOverlayFile))
			{
				Log(" Using overlay cache '%s'\n", cString::To8Char(sOverlayFile).c_str());
				return sOverlayFile;
			}
		}

		////////////////////////////////
		// Cache next to the source file
		if(cPlatform::FileExists(sCache)) return sCache;

		return _W("");
	}

	//-----------------------------------------------------------------------
}
