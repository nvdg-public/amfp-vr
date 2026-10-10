/*
 * VR package helpers (plan.md Phase 6b), shared by the game exe (Main.cpp) and the launcher (vrlauncher).
 * Header-only, Windows only, no engine dependencies. Part of the GPLv3 Amnesia: A Machine for Pigs VR mod.
 */
#pragma once
#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shobjidl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "user32.lib")

static bool LuxPkgFileExists(const std::wstring &asPath)
{
	DWORD lAttr = GetFileAttributesW(asPath.c_str());
	return lAttr != INVALID_FILE_ATTRIBUTES && (lAttr & FILE_ATTRIBUTE_DIRECTORY)==0;
}

static bool LuxPkgDirExists(const std::wstring &asPath)
{
	DWORD lAttr = GetFileAttributesW(asPath.c_str());
	return lAttr != INVALID_FILE_ATTRIBUTES && (lAttr & FILE_ATTRIBUTE_DIRECTORY)!=0;
}

static bool LuxPkgIsGameDir(const std::wstring &asDir)
{
	return asDir.empty()==false && LuxPkgFileExists(asDir + L"\\resources.cfg") && LuxPkgDirExists(asDir + L"\\maps");
}

static std::wstring LuxPkgFullPath(const std::wstring &asPath)
{
	wchar_t sBuf[MAX_PATH*2];
	DWORD lLen = GetFullPathNameW(asPath.c_str(), MAX_PATH*2, sBuf, NULL);
	if(lLen==0 || lLen>=MAX_PATH*2) return asPath;
	std::wstring sRet(sBuf);
	while(sRet.size()>3 && (sRet[sRet.size()-1]==L'\\' || sRet[sRet.size()-1]==L'/')) sRet.erase(sRet.size()-1);
	return sRet;
}

static std::wstring LuxPkgReadGameFolder(const std::wstring &asCfg)
{
	FILE *pFile = _wfopen(asCfg.c_str(), L"rb");
	if(pFile==NULL) return L"";
	std::string sData;
	char vBuf[1024];
	size_t lRead;
	while((lRead = fread(vBuf, 1, sizeof(vBuf), pFile)) > 0) sData.append(vBuf, lRead);
	fclose(pFile);

	const std::string sKey = "GameFolder=";
	size_t lPos = 0;
	while(lPos < sData.size())
	{
		size_t lEnd = sData.find_first_of("\r\n", lPos);
		if(lEnd==std::string::npos) lEnd = sData.size();
		std::string sLine = sData.substr(lPos, lEnd-lPos);
		lPos = lEnd+1;
		if(sLine.compare(0, sKey.size(), sKey)!=0) continue;

		std::string sVal = sLine.substr(sKey.size());
		if(sVal.empty()) return L"";
		int lWide = MultiByteToWideChar(CP_UTF8, 0, sVal.c_str(), (int)sVal.size(), NULL, 0);
		std::wstring sRet(lWide, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, sVal.c_str(), (int)sVal.size(), &sRet[0], lWide);
		return sRet;
	}
	return L"";
}

static void LuxPkgWriteGameFolder(const std::wstring &asCfg, const std::wstring &asGameDir)
{
	int lLen = WideCharToMultiByte(CP_UTF8, 0, asGameDir.c_str(), (int)asGameDir.size(), NULL, 0, NULL, NULL);
	std::string sDir(lLen, '\0');
	WideCharToMultiByte(CP_UTF8, 0, asGameDir.c_str(), (int)asGameDir.size(), &sDir[0], lLen, NULL, NULL);

	FILE *pFile = _wfopen(asCfg.c_str(), L"wb");
	if(pFile==NULL) return;
	fputs("# Amnesia: A Machine for Pigs VR package.\r\n"
		  "# GameFolder = where the game is installed (the folder with resources.cfg and maps).\r\n"
		  "# Leave it empty to be asked again on the next start.\r\n", pFile);
	fputs("GameFolder=", pFile);
	fputs(sDir.c_str(), pFile);
	fputs("\r\n", pFile);
	fclose(pFile);
}

static std::wstring LuxPkgPickGameFolder()
{
	std::wstring sRet;
	HRESULT hrInit = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	for(;;)
	{
		IFileOpenDialog *pDialog = NULL;
		if(FAILED(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDialog)))) break;

		DWORD lOptions = 0;
		pDialog->GetOptions(&lOptions);
		pDialog->SetOptions(lOptions | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
		pDialog->SetTitle(L"Where is Amnesia: A Machine for Pigs installed? (the folder with aamfp.exe)");

		std::wstring sPicked;
		bool bCancelled = true;
		if(SUCCEEDED(pDialog->Show(NULL)))
		{
			IShellItem *pItem = NULL;
			if(SUCCEEDED(pDialog->GetResult(&pItem)))
			{
				PWSTR pPath = NULL;
				if(SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pPath)))
				{
					sPicked = pPath;
					CoTaskMemFree(pPath);
					bCancelled = false;
				}
				pItem->Release();
			}
		}
		pDialog->Release();
		if(bCancelled) break;

		sPicked = LuxPkgFullPath(sPicked);
		if(LuxPkgIsGameDir(sPicked)) { sRet = sPicked; break; }

		MessageBoxW(NULL, (L"This folder does not contain the game (resources.cfg and the maps folder are missing):\n\n" + sPicked +
					L"\n\nPick the folder where Amnesia: A Machine for Pigs is installed.").c_str(),
					L"Amnesia: A Machine for Pigs VR", MB_OK | MB_ICONWARNING);
	}
	if(SUCCEEDED(hrInit)) CoUninitialize();
	return sRet;
}

static std::string LuxPkgToAnsi(const std::wstring &asText)
{
	int lLen = WideCharToMultiByte(CP_ACP, 0, asText.c_str(), (int)asText.size(), NULL, 0, NULL, NULL);
	std::string sRet(lLen, '\0');
	WideCharToMultiByte(CP_ACP, 0, asText.c_str(), (int)asText.size(), &sRet[0], lLen, NULL, NULL);
	return sRet;
}

static bool LuxPkgMakeDirs(const std::wstring &asDir)
{
	if(LuxPkgDirExists(asDir)) return true;
	size_t lSlash = asDir.find_last_of(L"\\/");
	if(lSlash != std::wstring::npos && lSlash > 2 && LuxPkgMakeDirs(asDir.substr(0, lSlash))==false) return false;
	return CreateDirectoryW(asDir.c_str(), NULL) || GetLastError()==ERROR_ALREADY_EXISTS;
}

/**
 * The game ships some FBX animations without the converted .anm the engine loads (plan.md Phase 1b). The package
 * lists them in vr_animations.txt (paths relative to the game folder); missing ones are converted with fbx2anm.exe
 * from the player's own game files into data_overlay next to the exe (or %LOCALAPPDATA%\AmnesiaAMFP_VR\data_overlay
 * if the package folder is read-only). Returns the overlay folder, empty if there is none.
 */
static std::wstring LuxPkgPrepareAnimations(const std::wstring &asExeDir, const std::wstring &asGameDir)
{
	std::wstring sOverlay = asExeDir + L"\\data_overlay";
	FILE *pList = _wfopen((asExeDir + L"\\vr_animations.txt").c_str(), L"rb");
	if(pList==NULL) return LuxPkgDirExists(sOverlay) ? sOverlay : L"";

	std::vector<std::wstring> vFiles;
	char vLine[1024];
	while(fgets(vLine, sizeof(vLine), pList))
	{
		std::string sLine(vLine);
		while(sLine.empty()==false && (sLine[sLine.size()-1]=='\n' || sLine[sLine.size()-1]=='\r' || sLine[sLine.size()-1]==' ')) sLine.erase(sLine.size()-1);
		if(sLine.empty() || sLine[0]=='#') continue;
		std::wstring sFile(sLine.begin(), sLine.end()); // paths in the list are plain ASCII
		for(size_t i=0; i<sFile.size(); ++i) if(sFile[i]==L'/') sFile[i] = L'\\';
		vFiles.push_back(sFile);
	}
	fclose(pList);

	// Writable overlay folder
	if(LuxPkgMakeDirs(sOverlay)==false)
	{
		const wchar_t *pLocal = _wgetenv(L"LOCALAPPDATA");
		if(pLocal==NULL) return L"";
		sOverlay = std::wstring(pLocal) + L"\\AmnesiaAMFP_VR\\data_overlay";
		if(LuxPkgMakeDirs(sOverlay)==false) return L"";
	}

	// Jobs: listed files the game has but the overlay lacks
	std::string sJobs;
	int lJobs = 0;
	for(size_t i=0; i<vFiles.size(); ++i)
	{
		std::wstring sIn = asGameDir + L"\\" + vFiles[i];
		std::wstring sOut = sOverlay + L"\\" + vFiles[i].substr(0, vFiles[i].find_last_of(L'.')) + L".anm";
		if(LuxPkgFileExists(sOut) || LuxPkgFileExists(sIn)==false) continue;
		LuxPkgMakeDirs(sOut.substr(0, sOut.find_last_of(L'\\')));
		sJobs += LuxPkgToAnsi(sIn) + "\t" + LuxPkgToAnsi(sOut) + "\r\n";
		++lJobs;
	}
	if(lJobs==0) return sOverlay;

	// Convert (about 3 s for all of them)
	wchar_t sTemp[MAX_PATH];
	GetTempPathW(MAX_PATH, sTemp);
	std::wstring sJobFile = std::wstring(sTemp) + L"amfp_vr_animations.txt";
	FILE *pJobs = _wfopen(sJobFile.c_str(), L"wb");
	if(pJobs==NULL) return sOverlay;
	fputs(sJobs.c_str(), pJobs);
	fclose(pJobs);

	std::wstring sCmd = L"\"" + asExeDir + L"\\fbx2anm.exe\" --list \"" + sJobFile + L"\"";
	STARTUPINFOW startInfo;
	ZeroMemory(&startInfo, sizeof(startInfo));
	startInfo.cb = sizeof(startInfo);
	PROCESS_INFORMATION procInfo;
	DWORD lExitCode = 1;
	std::vector<wchar_t> vCmd(sCmd.begin(), sCmd.end());
	vCmd.push_back(0);
	if(CreateProcessW(NULL, &vCmd[0], NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, asExeDir.c_str(), &startInfo, &procInfo))
	{
		WaitForSingleObject(procInfo.hProcess, 5*60*1000);
		GetExitCodeProcess(procInfo.hProcess, &lExitCode);
		CloseHandle(procInfo.hThread);
		CloseHandle(procInfo.hProcess);
	}
	DeleteFileW(sJobFile.c_str());

	if(lExitCode != 0)
	{
		MessageBoxW(NULL, L"Some of the game's character animations could not be converted, so some characters may not move.\n\n"
					L"The game will start anyway. The conversion is tried again on the next start.",
					L"Amnesia: A Machine for Pigs VR", MB_OK | MB_ICONWARNING);
	}
	return sOverlay;
}

/**
 * Game folder for a package: the current folder, the package's parent folder, the folder remembered in
 * vr_package.cfg, or a folder picker (the answer is saved). Empty if the user cancels (after a message).
 */
static std::wstring LuxPkgFindGameFolder(const std::wstring &asExeDir)
{
	std::wstring sCfg = asExeDir + L"\\vr_package.cfg";
	wchar_t sCwd[MAX_PATH*2];
	if(GetCurrentDirectoryW(MAX_PATH*2, sCwd) > 0 && LuxPkgIsGameDir(LuxPkgFullPath(sCwd))) return LuxPkgFullPath(sCwd);
	if(LuxPkgIsGameDir(LuxPkgFullPath(asExeDir + L"\\.."))) return LuxPkgFullPath(asExeDir + L"\\..");

	std::wstring sSaved = LuxPkgReadGameFolder(sCfg);
	if(LuxPkgIsGameDir(sSaved)) return sSaved;

	std::wstring sGameDir = LuxPkgPickGameFolder();
	if(sGameDir.empty())
	{
		MessageBoxW(NULL, L"The game folder was not chosen, so the game cannot start.\n\n"
					L"Start again and pick the folder where Amnesia: A Machine for Pigs is installed.",
					L"Amnesia: A Machine for Pigs VR", MB_OK | MB_ICONINFORMATION);
		return L"";
	}
	LuxPkgWriteGameFolder(sCfg, sGameDir);
	return sGameDir;
}

static std::wstring LuxPkgExeDir()
{
	wchar_t sExe[MAX_PATH*2];
	DWORD lLen = GetModuleFileNameW(NULL, sExe, MAX_PATH*2);
	if(lLen==0 || lLen>=MAX_PATH*2) return L"";
	std::wstring sDir(sExe);
	return sDir.substr(0, sDir.find_last_of(L"\\/"));
}

#endif
