// Modern Camera Collision -- third-person camera collision for Skyrim SE/AE
// Copyright (C) 2026 DisizMod
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

// Where the camera sits along its cast. The engine puts it at the hit
// every frame and pops it back the frame the hit is gone. Here the
// distance, as a fraction of the cast, eases toward the nearer of the
// engine's stop and the prediction rays' cap, is held after the way clears, and
// eases back out -- Cinemachine's damping-when-occluded, smoothing time
// and damping, in that order.

#include "Settings.h"

namespace mcc::motion
{
	// After the engine's collision, with this update's cast known.
	void Apply(RE::ThirdPersonState* a_state, bool a_castThisUpdate, const settings::Values& a_settings);

	// The fraction the camera is held at, for the log.
	[[nodiscard]] float Pull();
}
