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
