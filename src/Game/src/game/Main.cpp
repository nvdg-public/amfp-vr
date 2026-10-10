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

#include "LuxBase.h"

//---------------------------------------
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "LuxVRPackage.h"
#endif

//---------------------------------------

cLuxBase* gpBase = NULL;

//---------------------------------------

#ifdef WIN32
/**
 * VR package mode (plan.md Phase 6b): only when vr_package.cfg sits next to the exe. Finds the game folder
 * (current folder, the exe's parent folder, the folder remembered in vr_package.cfg, or a folder picker),
 * makes it the working folder, turns VR on (unless HPL_VR is set or "-flat" is given) and points
 * HPL_CACHE_OVERLAY at the package's data_overlay. Returns false if the user cancels.
 */
static bool LuxPkgSetup(const tString &asCommandline)
{
	std::wstring sExeDir = LuxPkgExeDir();
	if(sExeDir.empty() || LuxPkgFileExists(sExeDir + L"\\vr_package.cfg")==false) return true; // not a package (run\ folder, test launchers)

	// Game folder (normally the launcher already made it the working folder)
	std::wstring sGameDir = LuxPkgFindGameFolder(sExeDir);
	if(sGameDir.empty()) return false;
	SetCurrentDirectoryW(sGameDir.c_str());

	// VR on unless asked otherwise
	const char *pVR = getenv("HPL_VR");
	if((pVR==NULL || pVR[0]==0) && asCommandline.find("-flat")==tString::npos) _putenv_s("HPL_VR", "1");

	// FBX animations the game has no .anm for: converted from the player's own game files on first start
	const wchar_t *pOverlay = _wgetenv(L"HPL_CACHE_OVERLAY");
	if(pOverlay==NULL || pOverlay[0]==0)
	{
		std::wstring sOverlay = LuxPkgPrepareAnimations(sExeDir, sGameDir);
		if(sOverlay.empty()==false) _wputenv_s(L"HPL_CACHE_OVERLAY", sOverlay.c_str());
	}

	return true;
}
#endif

//---------------------------------------

int hplMain(const tString &asCommandline)
{
	#ifdef WIN32
		if(LuxPkgSetup(asCommandline)==false) return 0;
	#endif

	//////////////////////////
	// Init BlackBox
	#ifdef WIN32
		HINSTANCE hBlackBoxLib = LoadLibrary( "BlackBox.dll" );
	#endif

	//////////////////////////
	// Game creation and exit
	gpBase = hplNew( cLuxBase, ());
	
	//Init and run if all okay
	if(gpBase->Init(asCommandline))
	{
		
			gpBase->Run();
			gpBase->Exit();
		
	}
	//Error occurred
	else
	{
		if(gpBase->msErrorMessage==_W(""))
			gpBase->msErrorMessage = _W("Error occured");

		cPlatform::CreateMessageBox(_W("Error!"),gpBase->msErrorMessage.c_str());
		//No Exit, since it was not sure everything was created as it should.
	}

	hplDelete(gpBase);

	cMemoryManager::LogResults();

	//////////////////////////
	// Exit BlackBox
	#ifdef WIN32
			if(hBlackBoxLib) FreeLibrary(hBlackBoxLib);
	#endif

	return 0;
}
