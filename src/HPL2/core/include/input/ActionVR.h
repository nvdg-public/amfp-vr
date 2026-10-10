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

#ifndef HPL_ACTIONVR_H
#define HPL_ACTIONVR_H

#include "input/Action.h"
#include "vr/VRSystem.h"

namespace hpl {

	/** A VR controller button (cVRSystem) as an input for an action. Not saved in the key config. */
	class cActionVR : public iSubAction
	{
	public:
		cActionVR(eVRButton aButton) : mButton(aButton) {}

		bool IsTriggerd(){ return cVRSystem::Get()->GetButton(mButton); }
		float GetValue(){ return IsTriggerd() ? 1.0f : 0.0f; }

		tString GetInputName(){ return cVRSystem::GetButtonName(mButton); }
		tString GetInputType(){ return "VRController"; }

		eVRButton GetButton(){ return mButton; }
	private:
		eVRButton mButton;
	};

};
#endif // HPL_ACTIONVR_H
