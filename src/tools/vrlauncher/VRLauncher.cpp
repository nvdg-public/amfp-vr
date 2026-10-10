/*
 * Amnesia: A Machine for Pigs VR - launcher (aamfp_vr.exe). GPLv3, see LICENSE.
 *
 * The package ships no third-party game DLLs (plan.md Phase 6b). This small exe, which needs none of them,
 * finds the game folder (LuxVRPackage.h), copies the engine DLLs from the player's own game folder next to the
 * game exe when they are missing or changed, and starts aamfp_vr_game.exe with the game folder as working folder.
 * If the package folder is read-only the DLLs are not copied; the game folder is then put first on the PATH.
 */
#include "../../Game/src/game/LuxVRPackage.h"

static const wchar_t *gvGameDlls[] = {
	L"SDL2.dll", L"glew32.dll", L"DevIL.dll", L"Newton.dll", L"OpenAL32.dll", L"wrap_oal.dll", L"alut.dll",
	L"libogg.dll", L"libvorbis.dll", L"libvorbisfile.dll", L"libtheora.dll", L"zlibwapi.dll",
};

static const wchar_t *kTitle = L"Amnesia: A Machine for Pigs VR";

static bool SameFile(const std::wstring &asA, const std::wstring &asB)
{
	WIN32_FILE_ATTRIBUTE_DATA a, b;
	if(!GetFileAttributesExW(asA.c_str(), GetFileExInfoStandard, &a)) return false;
	if(!GetFileAttributesExW(asB.c_str(), GetFileExInfoStandard, &b)) return false;
	return a.nFileSizeHigh==b.nFileSizeHigh && a.nFileSizeLow==b.nFileSizeLow &&
		   CompareFileTime(&a.ftLastWriteTime, &b.ftLastWriteTime)==0;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
	std::wstring sExeDir = LuxPkgExeDir();
	std::wstring sGameExe = sExeDir + L"\\aamfp_vr_game.exe";
	if(LuxPkgFileExists(sGameExe)==false)
	{
		MessageBoxW(NULL, (L"aamfp_vr_game.exe is missing from\n\n" + sExeDir + L"\n\nCopy the whole VR folder again.").c_str(),
					kTitle, MB_OK | MB_ICONERROR);
		return 1;
	}

	std::wstring sGameDir = LuxPkgFindGameFolder(sExeDir);
	if(sGameDir.empty()) return 1;

	// Engine DLLs from the player's own game
	std::wstring sMissing;
	bool bCopyFailed = false;
	for(size_t i=0; i<sizeof(gvGameDlls)/sizeof(gvGameDlls[0]); ++i)
	{
		std::wstring sFrom = sGameDir + L"\\" + gvGameDlls[i];
		std::wstring sTo = sExeDir + L"\\" + gvGameDlls[i];
		if(LuxPkgFileExists(sFrom)==false) { sMissing += L"\n  "; sMissing += gvGameDlls[i]; continue; }
		if(SameFile(sFrom, sTo)) continue;
		if(!CopyFileW(sFrom.c_str(), sTo.c_str(), FALSE)) bCopyFailed = true;
	}
	if(sMissing.empty()==false)
	{
		MessageBoxW(NULL, (L"These files are missing from the game folder:\n" + sMissing + L"\n\nGame folder:\n" + sGameDir +
					L"\n\nCheck or repair the game installation.").c_str(), kTitle, MB_OK | MB_ICONERROR);
		return 1;
	}
	if(bCopyFailed)
	{
		// Read-only package folder: let Windows find the DLLs in the game folder instead
		std::vector<wchar_t> vPath(32767);
		DWORD lLen = GetEnvironmentVariableW(L"PATH", &vPath[0], (DWORD)vPath.size());
		std::wstring sPath = sGameDir + (lLen > 0 ? L";" + std::wstring(&vPath[0]) : L"");
		SetEnvironmentVariableW(L"PATH", sPath.c_str());
	}

	// Start the game with the same arguments
	std::wstring sArgs;
	LPWSTR pCmd = GetCommandLineW();
	if(*pCmd==L'"') { ++pCmd; while(*pCmd && *pCmd!=L'"') ++pCmd; if(*pCmd) ++pCmd; }
	else { while(*pCmd && *pCmd!=L' ' && *pCmd!=L'\t') ++pCmd; }
	sArgs = pCmd;

	std::wstring sCmd = L"\"" + sGameExe + L"\"" + sArgs;
	std::vector<wchar_t> vCmd(sCmd.begin(), sCmd.end());
	vCmd.push_back(0);
	STARTUPINFOW startInfo;
	ZeroMemory(&startInfo, sizeof(startInfo));
	startInfo.cb = sizeof(startInfo);
	PROCESS_INFORMATION procInfo;
	if(!CreateProcessW(sGameExe.c_str(), &vCmd[0], NULL, NULL, FALSE, 0, NULL, sGameDir.c_str(), &startInfo, &procInfo))
	{
		MessageBoxW(NULL, L"Could not start aamfp_vr_game.exe.", kTitle, MB_OK | MB_ICONERROR);
		return 1;
	}
	CloseHandle(procInfo.hThread);
	CloseHandle(procInfo.hProcess);
	return 0;
}
