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
