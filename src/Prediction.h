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

// Prediction rays, as Nesky's (50 Camera Mistakes, #5-#7, #12): rays from the
// pivot to where the camera would be at a yaw and a pitch off its line --
// horizontal, vertical and diagonal groups -- and one from the wanted
// position straight back. A blocked prediction ray says a wall is coming before
// the camera's own sweep meets it. The ones nearest the camera's line cap
// the distance at what they measure free (#7). The side ones can swing
// the camera's free rotation away from a blocked side (#5); that is off
// until the player's heading and input come into it (#6).
//
// Every prediction ray hit is judged by the collision's one rule, once per
// reference an update.

#include "Settings.h"

namespace mcc::prediction
{
	// Cast inside the engine's own sweep, where the physics world is held.
	void Cast(const RE::hkpWorld* a_world, float a_scale, const settings::Values& a_settings);

	// The fraction of the cast the near prediction rays found free, 1 when clear.
	[[nodiscard]] float NearFree();

	// The swing, applied after the engine placed the camera.
	void Swing(RE::ThirdPersonState* a_state, float a_dt, const settings::Values& a_settings);
}
