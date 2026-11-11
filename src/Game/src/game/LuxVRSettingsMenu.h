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

#ifndef LUX_VR_SETTINGS_MENU_H
#define LUX_VR_SETTINGS_MENU_H

#include "LuxBase.h"

//----------------------------------------------

/**
 * VR: in-game panel that edits vr_settings.cfg. Opened in the main menu / pause menu by clicking the left stick.
 *
 * A fixed table of rows (label, config element + attribute, a list of choices or a number range, default, and
 * whether it applies live, after a restart or when a map loads). Changes go into the in-memory config and are applied
 * at once (cVRSystem::ApplyLiveSettings, cLuxVRSettings::Reload, post effect overrides). The file is written when
 * the panel closes: only the changed attributes are replaced, in place, so comments and layout stay.
 *
 * Drawn with GDI into a 32-bit DIB, shown on the VR GUI panel with cVRSystem::SetGuiPanelOverlay (uploaded only when
 * something changed). Pointing: the laser / gaze / mouse (the cursor position), click = trigger or mouse; sticks
 * move the selection and change values; B (back) or the left stick click closes it.
 */
class cLuxVRSettingsMenu
{
public:
	static cLuxVRSettingsMenu* Get();

	/** Every logic step. abAllowed: the main menu (or pause menu) has the input. */
	void Update(bool abAllowed);

	bool IsOpen() const { return mbOpen; }
	/** True while open, and after closing until the buttons that closed it are let go (the menu below must not get them). */
	bool IsBlockingInput() const { return mbOpen || mbBlockUntilRelease; }

	void Open();
	void Close();

	struct cRow;

private:
	cLuxVRSettingsMenu();

	tString GetValue(int alRow);
	void SetValue(int alRow, const tString& asValue);
	void ChangeValue(int alRow, int alDir, bool abWrap);
	tWString GetValueText(int alRow);
	bool IsSelectable(int alRow);
	void MoveSelection(int alDir);
	void EnsureVisible();
	void ApplyLive();
	bool SaveFile();

	void Layout();
	void Redraw();
	void FreeDib();
	int RowAt(const cVector2l& avLocal);

	bool mbOpen;
	bool mbBlockUntilRelease;
	bool mbRunDown;
	int mlSelected;
	int mlScroll;
	int mlVisibleRows;
	bool mbDirty;		// picture must be redrawn
	tStringVec mvStartValues;	// values when the game started (restart notes)
	std::vector<bool> mvChanged;	// changed since the panel was opened (to write to the file)
	bool mbAnyChanged;

	cVector2l mvLastPointer;
	bool mbPointerValid;

	// Trigger/mouse held: click on release, or drag the list
	bool mbPressed, mbDragging;
	int mlPressY, mlPressScroll, mlPressRow;

	// Stick repeat
	int mlStickDirY, mlStickDirX;
	double mfStickNextTime;

	// Picture
	void *mpDC;
	void *mpBitmap;
	void *mpOldBitmap;
	void *mpBits;
	void *mpFontTitle, *mpFontRow, *mpFontSmall;
	int mlW, mlH;
	cVector2l mvPos;	// on the panel
	int mlRowH, mlListTop, mlValueX;
};

//----------------------------------------------

#endif // LUX_VR_SETTINGS_MENU_H
