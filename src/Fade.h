#pragma once

// What the camera is let through is faded: the trishapes under the hit
// are drawn at the fade alpha through a blend alpha property of ours, on
// a material of their own -- the engine shares one material between every
// shape with the same parameters, across meshes and references, and
// writing the shared one leaves other shapes faded. The alpha eases in and
// back out; the previous property returns when it arrives. Shapes that
// carry an alpha property of their own are left alone: their shadow pass
// multiplies texture alpha by material alpha, and a window's frame at 0.3
// stops casting the light shaft. A hook on the shadow pass shows faded
// shapes as they were, so a faded wall still casts.
//
// The same test that picks what to fade says what is "small" for the
// collision: what can be seen through can be passed through, and only that.

#include "Settings.h"

namespace mcc::fade
{
	// Hooks BSLightingShaderProperty's shadow-pass query.
	[[nodiscard]] bool Install();

	// Whether a shape is small at a_at: under the hit (its bound holds the
	// point, with a little slack for the collision mesh sitting off the
	// visible one), bound radius under the minimum, and fadeable.
	// a_toBound is the hit's distance to the bound's surface (<= 0 under).
	[[nodiscard]] bool WouldTake(RE::BSGeometry* a_geometry, const RE::NiPoint3& a_at, const settings::Values& a_settings,
		float& a_toBound, const char*& a_why);

	// Whether the reference has such a shape at a_at.
	[[nodiscard]] bool Fadeable(RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_at, const settings::Values& a_settings);

	// Fades the reference's shapes under a_at -- all of them when a_whole.
	// On the camera's thread, once per let-through hit.
	void Around(RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_at, bool a_whole, const settings::Values& a_settings);

	// After the update's Around calls: what was not picked starts back;
	// every fade takes a step.
	void Update(const settings::Values& a_settings);

	// Once a frame from the render side: when no camera update ran since
	// the last frame, everything faded is released, on the game's thread.
	void Tick();

	// The bound spheres of what is faded, for drawing.
	[[nodiscard]] std::vector<std::pair<RE::NiPoint3, float>> Bounds();
}
