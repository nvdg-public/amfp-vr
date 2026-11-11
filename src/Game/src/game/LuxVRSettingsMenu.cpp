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

#include "LuxVRSettingsMenu.h"
#include "LuxVR.h"
#include "LuxMapHandler.h"
#include "LuxMainMenu.h"

#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <stdio.h>
#include <math.h>

//////////////////////////////////////////////////////////////////////////
// THE TABLE
//////////////////////////////////////////////////////////////////////////

enum eRowType { eRowType_Header, eRowType_Bool, eRowType_Choice, eRowType_Number, eRowType_Close };
enum eRowApply { eRowApply_Live, eRowApply_Restart, eRowApply_MapLoad };

struct cLuxVRSettingsMenu::cRow
{
	eRowType mType;
	const wchar_t *msLabel;
	const char *msSection;
	const char *msKey;
	const char *msChoices;	// Choice: "A|B|C" (written to the file as they are)
	float mfMin, mfMax, mfStep;
	int mlDecimals;
	const char *msDefault;
	eRowApply mApply;
};

typedef cLuxVRSettingsMenu::cRow tRow;

#define HEADER(label)							{ eRowType_Header, label, NULL, NULL, NULL, 0,0,0, 0, NULL, eRowApply_Live }
#define BOOLROW(label, sec, key, def, apply)	{ eRowType_Bool, label, sec, key, NULL, 0,0,0, 0, def, apply }
#define CHOICE(label, sec, key, ch, def, apply)	{ eRowType_Choice, label, sec, key, ch, 0,0,0, 0, def, apply }
#define NUMBER(label, sec, key, mn, mx, st, dec, def, apply) { eRowType_Number, label, sec, key, NULL, mn, mx, st, dec, def, apply }

static const tRow gvRows[] = {
	HEADER(L"Turning and walking"),
	CHOICE(L"Stick turn",			"Locomotion", "StickTurn", "Snap|Smooth|Off", "Snap", eRowApply_Live),
	NUMBER(L"Snap angle",			"Locomotion", "SnapAngle", 10, 90, 5, 0, "30", eRowApply_Live),
	CHOICE(L"Mouse turn",			"Locomotion", "MouseTurn", "Smooth|Snap|Off", "Smooth", eRowApply_Live),
	BOOLROW(L"Room-scale",			"Locomotion", "RoomScale", "true", eRowApply_Live),
	NUMBER(L"Straight walk angle",	"Locomotion", "StraightAngle", 0, 44, 4, 0, "20", eRowApply_Live),

	HEADER(L"Comfort"),
	BOOLROW(L"Vignette",			"Vignette", "Enabled", "false", eRowApply_Live),
	NUMBER(L"Vignette strength",	"Vignette", "Strength", 0, 1, 0.1f, 1, "0.5", eRowApply_Live),
	BOOLROW(L"Head bob",			"Comfort", "HeadBob", "false", eRowApply_Live),
	NUMBER(L"Screen shake",			"Comfort", "ScreenShake", 0, 1, 0.1f, 1, "0.3", eRowApply_Live),
	CHOICE(L"Look-at",				"Comfort", "LookAt", "Snap|Smooth|Off", "Snap", eRowApply_Live),
	BOOLROW(L"Radial blur",			"Comfort", "RadialBlur", "false", eRowApply_Live),

	HEADER(L"Body"),
	BOOLROW(L"Real crouch",			"Body", "RealCrouch", "true", eRowApply_Live),
	BOOLROW(L"Real crouch keeps height", "Body", "RealCrouchKeepsHeight", "true", eRowApply_Live),
	NUMBER(L"Crouch depth (m)",		"Body", "CrouchDepth", 0.1f, 1.0f, 0.05f, 2, "0.35", eRowApply_Live),

	HEADER(L"Hands"),
	BOOLROW(L"Left-handed",			"Input", "LeftHanded", "false", eRowApply_Restart),
	CHOICE(L"Pointer",				"Input", "Pointer", "Hand|Head", "Hand", eRowApply_Live),
	BOOLROW(L"Show hands",			"Hands", "ShowHands", "true", eRowApply_Live),
	CHOICE(L"Lantern turn",			"Hands", "LanternTurn", "Point|Arm|Fixed|Hand", "Point", eRowApply_Live),
	NUMBER(L"Lantern hip radius (m)", "Hands", "LanternHipRadius", 0.05f, 1.0f, 0.05f, 2, "0.25", eRowApply_Live),
	BOOLROW(L"Haptics",				"Hands", "Haptics", "true", eRowApply_Live),
	BOOLROW(L"Off-hand interact",	"Hands", "OffHandInteract", "true", eRowApply_Live),
	NUMBER(L"Throw strength",		"Hands", "ThrowStrength", 0, 3, 0.25f, 2, "1.0", eRowApply_Live),

	HEADER(L"Menu panel"),
	NUMBER(L"Panel distance (m)",	"Menu", "Distance", 0.5f, 5, 0.25f, 2, "2.0", eRowApply_Live),
	NUMBER(L"Panel width (m)",		"Menu", "Width", 0.5f, 5, 0.25f, 2, "2.0", eRowApply_Live),
	CHOICE(L"Panel follows you",	"Menu", "Follow", "Lazy|Fixed", "Lazy", eRowApply_Live),
	BOOLROW(L"SteamVR keyboard",	"Menu", "Keyboard", "true", eRowApply_Live),
	NUMBER(L"Reticle size",			"Reticle", "Size", 0.25f, 3, 0.25f, 2, "1.0", eRowApply_Live),

	HEADER(L"Render and mirror"),
	NUMBER(L"Render scale",			"Render", "Scale", 0.5f, 2.0f, 0.1f, 1, "1.0", eRowApply_Restart),
	BOOLROW(L"Edge smoothing",		"Render", "EdgeSmooth", "false", eRowApply_Live),
	BOOLROW(L"Mirror window",		"Mirror", "Enabled", "true", eRowApply_Live),
	CHOICE(L"Mirror fit",			"Mirror", "Fit", "Fill|Fit", "Fill", eRowApply_Live),
	NUMBER(L"Mirror zoom",			"Mirror", "Zoom", 1, 4, 0.1f, 1, "1.0", eRowApply_Live),
	CHOICE(L"Mirror source",		"Mirror", "Source", "Game|Compositor", "Game", eRowApply_Live),
	BOOLROW(L"Diagnostics log",		"Diagnostics", "Log", "false", eRowApply_Live),

	{ eRowType_Close, L"Close", NULL, NULL, NULL, 0,0,0, 0, NULL, eRowApply_Live },
};
static const int glNumRows = sizeof(gvRows) / sizeof(gvRows[0]);

//////////////////////////////////////////////////////////////////////////
// HELPERS
//////////////////////////////////////////////////////////////////////////

static void SplitChoices(const char *asChoices, tStringVec &avOut)
{
	avOut.clear();
	tString sCur;
	for(const char *p = asChoices; ; ++p)
	{
		if(*p=='|' || *p==0) { avOut.push_back(sCur); sCur = ""; if(*p==0) break; }
		else sCur += *p;
	}
}

static tString FormatNumber(float afX, int alDecimals)
{
	char sBuf[32];
	sprintf(sBuf, "%.*f", alDecimals, afX);
	return sBuf;
}

static bool IsTrue(const tString& asX)
{
	tString s = cString::ToLowerCase(asX);
	return s=="true" || s=="1" || s=="yes" || s=="on";
}

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTOR
//////////////////////////////////////////////////////////////////////////

cLuxVRSettingsMenu* cLuxVRSettingsMenu::Get()
{
	static cLuxVRSettingsMenu gMenu;
	return &gMenu;
}

cLuxVRSettingsMenu::cLuxVRSettingsMenu()
{
	mbOpen = false;
	mbBlockUntilRelease = false;
	mbRunDown = false;
	mlSelected = 1;
	mlScroll = 0;
	mlVisibleRows = 20;
	mbDirty = true;
	mbAnyChanged = false;
	mvChanged.resize(glNumRows, false);
	mvLastPointer = cVector2l(-10000, -10000);
	mbPointerValid = false;
	mbPressed = mbDragging = false;
	mlPressY = mlPressScroll = 0; mlPressRow = -1;
	mlStickDirX = mlStickDirY = 0;
	mfStickNextTime = 0;
	mpDC = mpBitmap = mpOldBitmap = mpBits = NULL;
	mpFontTitle = mpFontRow = mpFontSmall = NULL;
	mlW = mlH = 0;
	mlRowH = 40; mlListTop = 0; mlValueX = 0;
}

//////////////////////////////////////////////////////////////////////////
// VALUES
//////////////////////////////////////////////////////////////////////////

tString cLuxVRSettingsMenu::GetValue(int alRow)
{
	const tRow &row = gvRows[alRow];
	if(row.msSection==NULL) return "";
	cConfigFile *pCfg = cVRSystem::Get()->GetConfig();
	if(pCfg==NULL) return row.msDefault;
	return pCfg->GetString(row.msSection, row.msKey, row.msDefault);
}

void cLuxVRSettingsMenu::SetValue(int alRow, const tString& asValue)
{
	const tRow &row = gvRows[alRow];
	cConfigFile *pCfg = cVRSystem::Get()->GetConfig();
	if(pCfg==NULL || row.msSection==NULL) return;
	if(GetValue(alRow) == asValue) return;
	pCfg->SetString(row.msSection, row.msKey, asValue);
	mvChanged[alRow] = true;
	mbAnyChanged = true;
	mbDirty = true;
	Log(" VR settings menu: %s %s = \"%s\"\n", row.msSection, row.msKey, asValue.c_str());
	ApplyLive();
}

void cLuxVRSettingsMenu::ChangeValue(int alRow, int alDir, bool abWrap)
{
	const tRow &row = gvRows[alRow];
	tString sCur = GetValue(alRow);
	switch(row.mType)
	{
	case eRowType_Bool:
		SetValue(alRow, IsTrue(sCur) ? "false" : "true");
		break;
	case eRowType_Choice:
		{
			tStringVec vChoices;
			SplitChoices(row.msChoices, vChoices);
			int lNum = (int)vChoices.size();
			int lIdx = -1;
			for(int i=0; i<lNum; ++i) if(cString::ToLowerCase(vChoices[i]) == cString::ToLowerCase(sCur)) lIdx = i;
			if(lIdx < 0)
			{
				lIdx = 0;
				for(int i=0; i<lNum; ++i) if(vChoices[i] == row.msDefault) lIdx = i;
			}
			int lNew = lIdx + alDir;
			if(abWrap) lNew = (lNew + lNum) % lNum;
			else lNew = (lNew < 0 ? 0 : (lNew > lNum-1 ? lNum-1 : lNew));
			SetValue(alRow, vChoices[lNew]);
		}
		break;
	case eRowType_Number:
		{
			float fCur = cString::ToFloat(sCur.c_str(), (float)atof(row.msDefault));
			// Snap onto the step grid, then step
			float fSteps = (fCur - row.mfMin) / row.mfStep;
			float fGrid = alDir > 0 ? floorf(fSteps + 1e-3f) + 1.0f : ceilf(fSteps - 1e-3f) - 1.0f;
			float fNew = row.mfMin + fGrid * row.mfStep;
			if(fNew > row.mfMax + 1e-4f) fNew = abWrap ? row.mfMin : row.mfMax;
			if(fNew < row.mfMin - 1e-4f) fNew = abWrap ? row.mfMax : row.mfMin;
			SetValue(alRow, FormatNumber(fNew, row.mlDecimals));
		}
		break;
	default: break;
	}
}

tWString cLuxVRSettingsMenu::GetValueText(int alRow)
{
	const tRow &row = gvRows[alRow];
	tString sCur = GetValue(alRow);
	if(row.mType == eRowType_Bool) return IsTrue(sCur) ? _W("On") : _W("Off");
	if(row.mType == eRowType_Number)
		return cString::To16Char(FormatNumber(cString::ToFloat(sCur.c_str(), (float)atof(row.msDefault)), row.mlDecimals));
	return cString::To16Char(sCur);
}

bool cLuxVRSettingsMenu::IsSelectable(int alRow)
{
	return alRow >= 0 && alRow < glNumRows && gvRows[alRow].mType != eRowType_Header;
}

void cLuxVRSettingsMenu::MoveSelection(int alDir)
{
	int lRow = mlSelected;
	for(int i=0; i<glNumRows; ++i)
	{
		lRow = (lRow + alDir + glNumRows) % glNumRows;
		if(IsSelectable(lRow)) break;
	}
	mlSelected = lRow;
	EnsureVisible();
	mbDirty = true;
}

void cLuxVRSettingsMenu::EnsureVisible()
{
	int lTop = mlSelected;
	if(lTop > 0 && gvRows[lTop-1].mType == eRowType_Header) --lTop;	// show the section's header too
	if(lTop < mlScroll) mlScroll = lTop;
	if(mlSelected >= mlScroll + mlVisibleRows) mlScroll = mlSelected - mlVisibleRows + 1;
	{ int lMax = glNumRows > mlVisibleRows ? glNumRows - mlVisibleRows : 0; mlScroll = mlScroll < 0 ? 0 : (mlScroll > lMax ? lMax : mlScroll); }
}

void cLuxVRSettingsMenu::ApplyLive()
{
	cVRSystem::Get()->ApplyLiveSettings();
	cLuxVRSettings::Reload();
	if(gpBase->mpMapHandler) gpBase->mpMapHandler->ApplyVRPostEffectOverrides();
}

//////////////////////////////////////////////////////////////////////////
// SAVING
//////////////////////////////////////////////////////////////////////////

/**
 * Sets Key="Value" on the first <Section ...> element outside comments in the file's text. Adds the attribute if the
 * element doesn't have it, and the element at the end if the file doesn't. Everything else stays as it is (comments,
 * order, spacing), like the installer's merge (install.ps1 Merge-VrSettings).
 */
static void SetAttributeInText(std::string &asText, const tString& asSection, const tString& asKey, const tString& asValue, const char *asNewLine)
{
	size_t i = 0;
	while(i < asText.size())
	{
		if(asText.compare(i, 4, "<!--") == 0)
		{
			size_t lEnd = asText.find("-->", i+4);
			if(lEnd == std::string::npos) break;
			i = lEnd + 3;
			continue;
		}
		if(asText[i] == '<' && asText.compare(i+1, asSection.size(), asSection) == 0)
		{
			size_t lAfter = i + 1 + asSection.size();
			char c = lAfter < asText.size() ? asText[lAfter] : 0;
			if(c==' ' || c=='\t' || c=='\r' || c=='\n' || c=='/' || c=='>')
			{
				size_t lTagEnd = asText.find('>', lAfter);
				if(lTagEnd == std::string::npos) break;
				// Find Key = "..." inside the tag
				size_t p = lAfter;
				while(p < lTagEnd)
				{
					p = asText.find(asKey, p);
					if(p == std::string::npos || p >= lTagEnd) break;
					char cBefore = asText[p-1];
					size_t q = p + asKey.size();
					while(q < lTagEnd && (asText[q]==' ' || asText[q]=='\t')) ++q;
					bool bWordStart = cBefore==' ' || cBefore=='\t' || cBefore=='\r' || cBefore=='\n';
					if(bWordStart && q < lTagEnd && asText[q]=='=')
					{
						++q;
						while(q < lTagEnd && (asText[q]==' ' || asText[q]=='\t')) ++q;
						if(q < lTagEnd && asText[q]=='"')
						{
							size_t lValEnd = asText.find('"', q+1);
							if(lValEnd != std::string::npos && lValEnd < lTagEnd)
							{
								asText.replace(q+1, lValEnd - (q+1), asValue);
								return;
							}
						}
					}
					p += asKey.size();
				}
				// Not there: add it before "/>" or ">"
				size_t lInsert = lTagEnd;
				if(lInsert > 0 && asText[lInsert-1] == '/') --lInsert;
				while(lInsert > lAfter && (asText[lInsert-1]==' ' || asText[lInsert-1]=='\t')) --lInsert;
				asText.insert(lInsert, " " + asKey + "=\"" + asValue + "\"");
				return;
			}
		}
		++i;
	}
	// No such element
	asText += asNewLine;
	asText += "<" + asSection + " " + asKey + "=\"" + asValue + "\" />" + asNewLine;
}

bool cLuxVRSettingsMenu::SaveFile()
{
	cConfigFile *pCfg = cVRSystem::Get()->GetConfig();
	if(pCfg==NULL || mbAnyChanged==false) return true;

	const tWString &sFile = pCfg->GetFileLocation();
	std::string sText;
	FILE *pFile = _wfopen(sFile.c_str(), L"rb");
	if(pFile)
	{
		char vBuf[4096];
		size_t lRead;
		while((lRead = fread(vBuf, 1, sizeof(vBuf), pFile)) > 0) sText.append(vBuf, lRead);
		fclose(pFile);
	}
	const char *sNewLine = sText.find("\r\n") != std::string::npos || sText.empty() ? "\r\n" : "\n";

	for(int i=0; i<glNumRows; ++i)
	{
		if(mvChanged[i]==false) continue;
		SetAttributeInText(sText, gvRows[i].msSection, gvRows[i].msKey, GetValue(i), sNewLine);
	}

	pFile = _wfopen(sFile.c_str(), L"wb");
	if(pFile==NULL)
	{
		Warning("VR settings menu: could not write '%s'\n", cString::To8Char(sFile).c_str());
		return false;
	}
	fwrite(sText.data(), 1, sText.size(), pFile);
	fclose(pFile);
	Log(" VR settings menu: saved '%s'\n", cString::To8Char(sFile).c_str());

	for(int i=0; i<glNumRows; ++i) mvChanged[i] = false;
	mbAnyChanged = false;
	return true;
}

//////////////////////////////////////////////////////////////////////////
// OPEN / CLOSE / UPDATE
//////////////////////////////////////////////////////////////////////////

void cLuxVRSettingsMenu::Open()
{
	cVRSystem *pVR = cVRSystem::Get();
	if(mbOpen || pVR->IsActive()==false || pVR->GetConfig()==NULL) return;

	if(mvStartValues.empty())
	{
		mvStartValues.resize(glNumRows);
		for(int i=0; i<glNumRows; ++i) mvStartValues[i] = GetValue(i);
	}
	mbOpen = true;
	mbDirty = true;
	mbPointerValid = false;
	mbPressed = mbDragging = false;
	if(IsSelectable(mlSelected)==false) MoveSelection(1);
	Layout();
	EnsureVisible();
	if(gpBase->mpMainMenu && gpBase->mpMainMenu->GetSet()) gpBase->mpMainMenu->GetSet()->SetDrawMouse(false);
	Log(" VR settings menu: opened\n");
}

void cLuxVRSettingsMenu::Close()
{
	if(mbOpen==false) return;
	mbOpen = false;
	mbBlockUntilRelease = true;
	SaveFile();
	cVRSystem::Get()->SetGuiPanelOverlay(NULL, 0, 0, 0, false);
	FreeDib();
	if(gpBase->mpMainMenu && gpBase->mpMainMenu->GetSet()) gpBase->mpMainMenu->GetSet()->SetDrawMouse(true);
	Log(" VR settings menu: closed\n");
}

void cLuxVRSettingsMenu::Update(bool abAllowed)
{
	cVRSystem *pVR = cVRSystem::Get();
	if(pVR->IsActive()==false) return;
	cInput *pInput = gpBase->mpEngine->GetInput();

	// Left stick click (the Run button) opens and closes
	bool bRunDown = pVR->HasControllerInput() && pVR->GetButton(eVRButton_Run);
	bool bRunPressed = bRunDown && mbRunDown==false;
	mbRunDown = bRunDown;

	if(mbBlockUntilRelease)
	{
		if(pInput->IsTriggerd(eLuxAction_LeftClick)==false && pInput->IsTriggerd(eLuxAction_Exit)==false)
			mbBlockUntilRelease = false;
	}

	if(mbOpen==false)
	{
		if(abAllowed && bRunPressed && mbBlockUntilRelease==false) Open();
		if(mbOpen==false) return;
		bRunPressed = false;
	}
	if(abAllowed==false || bRunPressed || pInput->BecameTriggerd(eLuxAction_Exit))
	{
		Close();
		return;
	}

	////////////////////////////////
	// Pointer (laser / gaze / mouse: the cursor, in panel pixels)
	cVector2l vPointer = pInput->GetMouse()->GetAbsPosition();
	cVector2l vLocal = vPointer - mvPos;
	bool bInside = vLocal.x >= 0 && vLocal.y >= 0 && vLocal.x < mlW && vLocal.y < mlH;
	int lPointerRow = bInside ? RowAt(vLocal) : -1;
	if(bInside && mbDragging==false && (mbPointerValid==false || (vPointer - mvLastPointer).SqrLength() > 9))
	{
		if(lPointerRow >= 0 && IsSelectable(lPointerRow) && lPointerRow != mlSelected)
		{
			mlSelected = lPointerRow;
			mbDirty = true;
		}
		mvLastPointer = vPointer;
		mbPointerValid = true;
	}
	pVR->SetGuiPanelOverlayCursor(bInside, vPointer);

	////////////////////////////////
	// Click and drag (trigger or mouse). Holding it and moving up/down drags the list (like scrolling a phone);
	// letting go without having dragged is a click on the row it was pressed on.
	if(pInput->BecameTriggerd(eLuxAction_LeftClick) && bInside)
	{
		mbPressed = true;
		mbDragging = false;
		mlPressY = vPointer.y;
		mlPressScroll = mlScroll;
		mlPressRow = lPointerRow;
	}
	if(mbPressed && pInput->IsTriggerd(eLuxAction_LeftClick))
	{
		int lDy = vPointer.y - mlPressY;
		if(mbDragging==false && cMath::Abs(lDy) > mlRowH / 2) mbDragging = true;
		if(mbDragging)
		{
			// Content follows the pointer: moving it up shows the rows further down
			int lMax = glNumRows > mlVisibleRows ? glNumRows - mlVisibleRows : 0;
			int lScroll = mlPressScroll - (int)floorf((float)lDy / (float)mlRowH + 0.5f);
			lScroll = lScroll < 0 ? 0 : (lScroll > lMax ? lMax : lScroll);
			if(lScroll != mlScroll) { mlScroll = lScroll; mbDirty = true; }
		}
	}
	else if(mbPressed)
	{
		// Released
		bool bClick = mbDragging==false && lPointerRow >= 0 && lPointerRow == mlPressRow && IsSelectable(lPointerRow);
		mbPressed = false;
		mbDragging = false;
		mbPointerValid = false;	// the row under the pointer gets selected again
		if(bClick)
		{
			mlSelected = lPointerRow;
			mbDirty = true;
			if(gvRows[lPointerRow].mType == eRowType_Close) { Close(); return; }
			// The left half of the value (the < arrow) goes back, anywhere else forward
			int lValueMid = mlValueX + (mlW - mlValueX) / 2;
			bool bBack = vLocal.x >= mlValueX && vLocal.x < lValueMid;
			ChangeValue(lPointerRow, bBack ? -1 : 1, true);
		}
	}

	////////////////////////////////
	// Mouse wheel scrolls
	if(pInput->GetMouse()->ButtonIsDown(eMouseButton_WheelUp))	{ mlScroll = cMath::Max(0, mlScroll-1); mbDirty = true; }
	if(pInput->GetMouse()->ButtonIsDown(eMouseButton_WheelDown))	{ mlScroll = cMath::Min(cMath::Max(0, glNumRows - mlVisibleRows), mlScroll+1); mbDirty = true; }

	////////////////////////////////
	// Sticks (either): up/down select, left/right change. Repeat while held.
	cVector2f vStick = pVR->GetMoveAxis();
	cVector2f vTurn = pVR->GetTurnAxis();
	if(cMath::Abs(vTurn.x) + cMath::Abs(vTurn.y) > cMath::Abs(vStick.x) + cMath::Abs(vStick.y)) vStick = vTurn;
	int lDirY = 0, lDirX = 0;
	if(cMath::Abs(vStick.y) > 0.6f && cMath::Abs(vStick.y) > cMath::Abs(vStick.x)) lDirY = vStick.y > 0 ? -1 : 1;	// up = previous row
	else if(cMath::Abs(vStick.x) > 0.6f) lDirX = vStick.x > 0 ? 1 : -1;
	double fNow = cPlatform::GetApplicationTime() / 1000.0;
	bool bNew = lDirY != mlStickDirY || lDirX != mlStickDirX;
	if((lDirY || lDirX) && (bNew || fNow >= mfStickNextTime))
	{
		mfStickNextTime = fNow + (bNew ? 0.4 : 0.12);
		if(lDirY) MoveSelection(lDirY);
		else if(IsSelectable(mlSelected) && gvRows[mlSelected].mType != eRowType_Close) ChangeValue(mlSelected, lDirX, false);
		mbPointerValid = false;	// a resting laser doesn't take the selection back
		if(bInside) { mvLastPointer = vPointer; mbPointerValid = true; }
	}
	mlStickDirY = lDirY; mlStickDirX = lDirX;

	if(mbDirty) Redraw();
	pVR->SetGuiPanelOverlay(mpBits, mlW, mlH, mvPos, mbDirty);
	mbDirty = false;
}

//////////////////////////////////////////////////////////////////////////
// DRAWING (GDI)
//////////////////////////////////////////////////////////////////////////

int cLuxVRSettingsMenu::RowAt(const cVector2l& avLocal)
{
	if(avLocal.y < mlListTop) return -1;
	int lRow = (avLocal.y - mlListTop) / mlRowH;
	if(lRow >= mlVisibleRows) return -1;
	lRow += mlScroll;
	return lRow < glNumRows ? lRow : -1;
}

void cLuxVRSettingsMenu::FreeDib()
{
#ifdef WIN32
	if(mpDC)
	{
		SelectObject((HDC)mpDC, (HGDIOBJ)mpOldBitmap);
		DeleteDC((HDC)mpDC);
	}
	if(mpBitmap) DeleteObject((HGDIOBJ)mpBitmap);
	if(mpFontTitle) DeleteObject((HGDIOBJ)mpFontTitle);
	if(mpFontRow) DeleteObject((HGDIOBJ)mpFontRow);
	if(mpFontSmall) DeleteObject((HGDIOBJ)mpFontSmall);
#endif
	mpDC = mpBitmap = mpOldBitmap = mpBits = NULL;
	mpFontTitle = mpFontRow = mpFontSmall = NULL;
	mlW = mlH = 0;
}

void cLuxVRSettingsMenu::Layout()
{
#ifdef WIN32
	cVector2l vPanel = cVRSystem::Get()->GetRenderSize();
	int lW = cMath::Min((int)(vPanel.x * 0.64f), 1800);
	int lH = cMath::Min((int)(vPanel.y * 0.86f), (int)(lW * 1.45f));
	lW &= ~3;
	if(lW == mlW && lH == mlH && mpDC) return;
	FreeDib();
	mlW = lW; mlH = lH;
	mvPos = cVector2l((vPanel.x - mlW) / 2, (vPanel.y - mlH) / 2);

	BITMAPINFO bi;
	memset(&bi, 0, sizeof(bi));
	bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bi.bmiHeader.biWidth = mlW;
	bi.bmiHeader.biHeight = mlH;	// positive = bottom-up rows (the GL texture's v=0 is the bottom)
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biCompression = BI_RGB;
	HDC dc = CreateCompatibleDC(NULL);
	void *pBits = NULL;
	HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &pBits, NULL, 0);
	if(bmp==NULL || pBits==NULL)
	{
		DeleteDC(dc);
		mlW = mlH = 0;
		Warning("VR settings menu: CreateDIBSection failed\n");
		return;
	}
	mpDC = dc; mpBitmap = bmp; mpBits = pBits;
	mpOldBitmap = SelectObject(dc, bmp);

	mlRowH = mlH / 26;
	mlListTop = (int)(mlRowH * 2.6f);
	mlVisibleRows = (mlH - mlListTop - (int)(mlRowH * 1.4f)) / mlRowH;
	mlValueX = (int)(mlW * 0.58f);

	mpFontTitle = CreateFontW(-(int)(mlRowH * 0.95f), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
	mpFontRow = CreateFontW(-(int)(mlRowH * 0.6f), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
	mpFontSmall = CreateFontW(-(int)(mlRowH * 0.42f), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
	mbDirty = true;
#endif
}

#ifdef WIN32
static void FillRectColor(HDC aDC, int x0, int y0, int x1, int y1, COLORREF aCol)
{
	RECT r = { x0, y0, x1, y1 };
	HBRUSH br = CreateSolidBrush(aCol);
	FillRect(aDC, &r, br);
	DeleteObject(br);
}

static void Text(HDC aDC, HGDIOBJ aFont, COLORREF aCol, const wchar_t *asText, int x0, int y0, int x1, int y1, UINT alFlags)
{
	RECT r = { x0, y0, x1, y1 };
	SelectObject(aDC, aFont);
	SetTextColor(aDC, aCol);
	DrawTextW(aDC, asText, -1, &r, alFlags | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
}

static int TextWidth(HDC aDC, HGDIOBJ aFont, const wchar_t *asText)
{
	SelectObject(aDC, aFont);
	SIZE sz;
	GetTextExtentPoint32W(aDC, asText, (int)wcslen(asText), &sz);
	return sz.cx;
}
#endif

void cLuxVRSettingsMenu::Redraw()
{
#ifdef WIN32
	if(mpDC==NULL) Layout();
	if(mpDC==NULL) return;
	HDC dc = (HDC)mpDC;
	HGDIOBJ fTitle = (HGDIOBJ)mpFontTitle, fRow = (HGDIOBJ)mpFontRow, fSmall = (HGDIOBJ)mpFontSmall;

	const COLORREF colBg = RGB(20, 21, 26), colBorder = RGB(90, 94, 110), colSel = RGB(58, 64, 86);
	const COLORREF colText = RGB(228, 228, 232), colDim = RGB(135, 138, 150), colHeader = RGB(150, 172, 215);
	const COLORREF colAmber = RGB(255, 176, 32), colValue = RGB(255, 255, 255), colArrow = RGB(150, 160, 190);

	int lPad = mlRowH / 2;
	int lBorder = cMath::Max(2, mlRowH / 16);
	FillRectColor(dc, 0, 0, mlW, mlH, colBorder);
	FillRectColor(dc, lBorder, lBorder, mlW - lBorder, mlH - lBorder, colBg);
	SetBkMode(dc, TRANSPARENT);

	// Title and how to use it
	Text(dc, fTitle, colText, L"VR settings", lPad, (int)(mlRowH*0.2f), mlW - lPad, (int)(mlRowH*1.4f), DT_LEFT);
	Text(dc, fSmall, colDim, L"Point and click the trigger to change. Hold it and move up/down to scroll. Sticks work too.   Close: B or click the left stick.",
		lPad, (int)(mlRowH*1.4f), mlW - lPad, (int)(mlRowH*2.2f), DT_LEFT | DT_END_ELLIPSIS);

	// Rows
	int lRight = mlW - lPad - mlRowH / 3;	// room for the scroll bar
	for(int v=0; v<mlVisibleRows; ++v)
	{
		int lRow = mlScroll + v;
		if(lRow >= glNumRows) break;
		const tRow &row = gvRows[lRow];
		int y0 = mlListTop + v * mlRowH, y1 = y0 + mlRowH;

		if(row.mType == eRowType_Header)
		{
			Text(dc, fSmall, colHeader, row.msLabel, lPad, y0 + mlRowH/4, lRight, y1, DT_LEFT);
			continue;
		}
		if(lRow == mlSelected) FillRectColor(dc, lPad/2, y0 + 2, lRight + lPad/4, y1 - 2, colSel);

		if(row.mType == eRowType_Close)
		{
			Text(dc, fRow, colText, row.msLabel, lPad, y0, lRight, y1, DT_CENTER);
			continue;
		}

		int lLabelX = lPad + mlRowH / 3;
		Text(dc, fRow, colText, row.msLabel, lLabelX, y0, mlValueX, y1, DT_LEFT | DT_END_ELLIPSIS);

		// Note beside the label for settings that don't apply live
		if(row.mApply != eRowApply_Live)
		{
			bool bChanged = mvStartValues.size() == (size_t)glNumRows && GetValue(lRow) != mvStartValues[lRow];
			const wchar_t *sNote = row.mApply == eRowApply_Restart ?
				(bChanged ? L"restart the game to apply" : L"needs a restart") :
				(bChanged ? L"load a save to apply" : L"applies when a map loads");
			int lNoteX = lLabelX + TextWidth(dc, fRow, row.msLabel) + mlRowH / 3;
			Text(dc, fSmall, bChanged ? colAmber : colDim, sNote, lNoteX, y0, mlValueX - mlRowH/4, y1, DT_LEFT | DT_END_ELLIPSIS);
		}

		// < value >
		tWString sValue = GetValueText(lRow);
		Text(dc, fRow, colArrow, L"\x2039", mlValueX, y0, lRight, y1, DT_LEFT);
		Text(dc, fRow, colArrow, L"\x203A", mlValueX, y0, lRight, y1, DT_RIGHT);
		Text(dc, fRow, colValue, sValue.c_str(), mlValueX + mlRowH/2, y0, lRight - mlRowH/2, y1, DT_CENTER | DT_END_ELLIPSIS);
	}

	// Scroll bar
	if(glNumRows > mlVisibleRows)
	{
		int lTrackTop = mlListTop, lTrackH = mlVisibleRows * mlRowH;
		int lBarX0 = mlW - lPad/2 - mlRowH/6, lBarX1 = mlW - lPad/2;
		FillRectColor(dc, lBarX0, lTrackTop, lBarX1, lTrackTop + lTrackH, RGB(40, 42, 52));
		int lThumbTop = lTrackTop + lTrackH * mlScroll / glNumRows;
		int lThumbH = lTrackH * mlVisibleRows / glNumRows;
		FillRectColor(dc, lBarX0, lThumbTop, lBarX1, lThumbTop + lThumbH, colArrow);
	}

	// Footer
	Text(dc, fSmall, mbAnyChanged ? colText : colDim,
		mbAnyChanged ? L"Changed settings are saved to vr_settings.cfg when you close this panel." : L"Settings are saved to vr_settings.cfg when you close this panel.",
		lPad, mlH - (int)(mlRowH*1.3f), mlW - lPad, mlH - lBorder - mlRowH/6, DT_LEFT | DT_END_ELLIPSIS);

	// GDI leaves alpha at 0: make the picture opaque
	GdiFlush();
	unsigned char *pPix = (unsigned char*)mpBits;
	for(int i=0, n=mlW*mlH; i<n; ++i) pPix[i*4+3] = 255;
#endif
}
