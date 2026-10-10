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

#ifndef HPL_MESH_LOADER_FBX_CACHE_H
#define HPL_MESH_LOADER_FBX_CACHE_H

#include "resources/MeshLoader.h"

namespace hpl {

	class cMeshLoaderMSH;

	//------------------------------------------------------------

	/**
	 * Stand-in for cMeshLoaderFBX in builds without the FBX SDK (HPL_NO_FBX).
	 * Handles ".fbx" by loading the cached ".msh" / ".anm" that cMeshLoaderFBX would have used with
	 * ForceCacheLoadingAndSkipSaving. The cache is looked up first in the overlay directory named by the
	 * HPL_CACHE_OVERLAY environment variable (same relative path as the source file), then next to the
	 * source file. It never writes anything.
	 */
	class cMeshLoaderFBXCache : public iMeshLoader
	{
	public:
		cMeshLoaderFBXCache(iLowLevelGraphics *apLowLevelGraphics, cMeshLoaderMSH *apMeshLoaderMSH);
		~cMeshLoaderFBXCache();

		cMesh* LoadMesh(const tWString& asFile, tMeshLoadFlag aFlags);
		bool SaveMesh(cMesh* apMesh,const tWString& asFile){ return false; }

		cAnimation* LoadAnimation(const tWString& asFile);
		bool SaveAnimation(cAnimation* apAnimation, const tWString& asFile){ return false; }

	private:
		tWString GetCacheFile(const tWString& asFile, const tWString& asExt);

		cMeshLoaderMSH *mpMeshLoaderMSH;
		tWString msOverlayDir;
		tWString msWorkingDir;
	};

	//------------------------------------------------------------

};
#endif // HPL_MESH_LOADER_FBX_CACHE_H
