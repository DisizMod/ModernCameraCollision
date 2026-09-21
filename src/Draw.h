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

// Lines over the game, for seeing what the camera measured: the engine's
// sweep, the discs and their samples, the prediction rays, the faded shapes'
// bounds. A hook on the swap chain's Present draws them with a line
// shader of our own -- no ImGui -- from the last update's frame, projected
// through the player camera. Which of them are drawn is set from the MCM.
// The same hook is the once-a-frame tick the fade needs.

namespace mcc::draw
{
	// Installed once the renderer exists (data loaded).
	[[nodiscard]] bool Install();
}
