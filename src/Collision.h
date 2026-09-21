#pragma once

// The third-person camera's collision, edited before the engine sorts it.
//
// ThirdPersonState::UpdateCameraCollision does one sphere sweep per frame
// from the pivot to the wanted position, on the L_CAMERA layer, and every
// hit arrives unsorted at an all-hits collector. The sweep is fronted by a
// collector of ours that judges each hit -- the layer's rule, then the
// disc and the size -- and forwards only what stops the camera; the
// engine takes the next-closest of what is left. What was let through is
// faded. The prediction rays, the distance's easing and the drawing hang off the
// same update.

#include "Settings.h"

namespace mcc::collision
{
	[[nodiscard]] bool Install();

	// --- the verdict ----------------------------------------------------------
	//
	// Whether a hit stops the camera. One rule for the camera's own hits
	// and the prediction rays' alike:
	//   - the player's own body: never (the engine's to ignore)
	//   - terrain and ground: always
	//   - a layer set to "through": never, and the occluder is faded whole
	//   - a layer set to "stop": always
	//   - otherwise: only when enough of the disc's rays from the player
	//     land on the occluder (with hysteresis per reference) AND the
	//     shape under the hit is not small
	struct Verdict
	{
		bool stops = true;
		bool whole = false;  // let through by its layer: faded whole
		bool isSmall = false;  // the shape under the hit was small
		int  cover = -1;     // percent of the disc, -1 when not measured
	};

	// a_eye is where the camera would be: its wanted position for its own
	// hits, a prediction ray's far end for that prediction ray's.
	Verdict Judge(const RE::hkpWorld* a_world, const RE::hkpCollidable* a_root, RE::TESObjectREFR* a_ref,
		const RE::NiPoint3& a_at, const RE::NiPoint3& a_eye, float a_scale, bool a_forPrediction,
		const settings::Values& a_settings);

	// --- this update's cast, for the prediction rays and the motion ------------------
	[[nodiscard]] const RE::NiPoint3& CastFrom();
	[[nodiscard]] const RE::NiPoint3& CastTo();
	[[nodiscard]] float               CastLength();

	// Whether this update's judgements are being logged, and a line for one.
	[[nodiscard]] bool        Verbose();
	[[nodiscard]] std::string Describe(const RE::hkpCollidable* a_collidable);

	// A ray with the camera's own filter, in game units.
	void CastRay(const RE::hkpWorld* a_world, const RE::NiPoint3& a_from, const RE::NiPoint3& a_to, float a_scale,
		RE::hkpWorldRayCastOutput& a_output);

	// --- what was cast, for drawing --------------------------------------------
	enum class RayKind : std::uint8_t
	{
		Cast,     // the engine's own sweep, ending where the camera was put
		Prediction,
		Disc,     // a sample of the disc, from the player
	};

	struct RayView
	{
		RE::NiPoint3 from;
		RE::NiPoint3 to;
		RE::NiPoint3 hitAt;
		bool         hit;
		bool         counts;      // a prediction ray: the camera would stop there; a sample: the occluder was there
		bool         forPrediction;  // a sample cast for a prediction ray's hit
		RayKind      kind;
	};

	struct DiscView
	{
		RE::NiPoint3 at;
		RE::NiPoint3 outline[32];  // the bumper's edge, or the disc's, for drawing
		bool         dropped;
		bool         forPrediction;
		int          samples;
		RE::NiPoint3 point[64];
		bool         hit[64];
	};

	struct Frame
	{
		std::uint32_t                               update = 0;
		std::vector<RayView>                        rays;
		std::vector<DiscView>                       discs;
		std::vector<std::pair<RE::NiPoint3, float>> bounds;  // of what is faded
	};

	// A copy of the last update's frame, for the render side.
	[[nodiscard]] Frame LastFrame();

	// Whether rays are being kept this update (any drawing on).
	[[nodiscard]] bool Recording();
	void               Record(RayKind a_kind, const RE::NiPoint3& a_from, const RE::NiPoint3& a_to,
					  const RE::hkpWorldRayCastOutput& a_output, bool a_counts, bool a_forPrediction);
}
