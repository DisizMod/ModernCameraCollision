#pragma once

// Every number the mod runs on, read from MCM Helper's files: the mod's
// defaults in Data/MCM/Config/ModernCameraCollision/settings.ini, the
// user's values over them in Data/MCM/Settings/ModernCameraCollision.ini.
// Re-read when the MCM script sends the "MCC_SettingsChanged" mod event
// on closing the menu. Keys are MCM Helper's: a type letter, the name, and
// the section -- fHoldSecs in [Motion] is "fHoldSecs:Motion" in config.json.

namespace mcc::settings
{
	// What the camera does with a collision layer.
	enum class Rule : int
	{
		Measure = 0,  // the disc and the size decide
		Stop = 1,     // always stops the camera
		Through = 2,  // never stops it; not faded
		Fade = 3,     // never stops it; faded whole, whatever its size
	};

	struct Values
	{
		// [General]
		bool enabled = true;

		// [Motion]: the distance's way in and out
		float holdSecs = 0.3f;     // the pulled-in distance is kept this long after the way clears
		float easeInSecs = 0.06f;  // time constant of the way in
		float easeOutSecs = 0.25f; // ... and of the way back out

		// [Rules]
		float minBound = 100.0f;     // small: the shape under the hit with a bound radius under this
		// The body, as the camera sees it: a stadium -- a rectangle with
		// rounded ends -- upright in the plane facing the eye, about the
		// pivot the sweep starts from. Half-width for the shoulders, a
		// height above the pivot for the head, one below for the legs.
		float bodyHalfWidth = 30.0f;
		float bodyAbove = 30.0f;
		float bodyBelow = 100.0f;
		int   bodyColumns = 5;       // sample grid across the body...
		int   bodyRows = 5;          // ... and down it; points outside the stadium are left out
		// A horizontal line of samples at the pivot's height, across the
		// camera's line, reaching out past the body on both sides: what is
		// beside the player, for telling a pole from a wall.
		float sideReach = 120.0f;    // how far past the body's edge the line reaches
		int   sidePoints = 3;        // samples per side on it
		int   sideRows = 1;          // how many such lines, spread over the bumper's height as the grid's rows are
		// From a high (or low) angle -- the eye more than highAngle above
		// or below level -- the body is seen end-on: the samples cover only
		// the cap that faces the eye, at highScale of the width, and the
		// side line becomes a cross, level, along and across the eye's
		// direction.
		float highAngle = 45.0f;
		float highScale = 0.6f;
		int   highPoints = 8;        // points round the high-angle disc's edge
		int   dropBelowPercent = 75; // dropped when the occluder takes less of the disc than this
		float partDistance = 80.0f;  // a hit on the occluder counts only within this distance of the sweep's hit: the same part of it

		// [Prediction]
		bool  predictionRays = true;
		bool  predictionVertical = true;
		bool  predictionDiagonal = true;
		bool  shorten = true;          // the near prediction rays cap the distance at what they measure free
		bool  swing = false;           // turn the camera away from a blocked side (off: needs the player's heading)
		float swingYawDegPerSec = 60.0f;
		float swingPitchDegPerSec = 40.0f;
		bool  swingYawInverted = true;
		bool  swingPitchInverted = true;
		bool  intentWins = true;       // no swing while the player turns the camera

		// [Fade]
		bool  fade = true;
		float fadeAlpha = 0.3f;
		float fadeSecs = 0.2f;
		bool  fadeOwnAlpha = false;    // fade shapes that carry their own alpha property (a window: its shadow goes)
		bool  keepShadows = true;      // the shadow pass sees faded shapes as they were

		// [Layers]: a rule per collision layer, by the COLL record's name
		std::unordered_map<std::string, Rule> layerRules;

		// Overrides, from Data/SKSE/Plugins/ModernCameraCollision/Overrides/
		// *.ini: a rule for one object that wins over its layer's. By the
		// base object's resolved form id, or by its model path, lowercase.
		std::unordered_map<std::uint32_t, Rule> formOverrides;
		std::unordered_map<std::string, Rule>   modelOverrides;

		// [Debug]
		bool drawDiscs = false;
		bool drawPredictionRays = false;
		bool drawRays = false;
		bool drawBounds = false;
		bool logVerbose = false;  // one update in sixty, and the first ten, in full
	};

	// A copy of what is set right now. Cheap; taken once per camera update.
	[[nodiscard]] Values Current();

	// Reads the two files, and the override folder. Missing files are the
	// defaults; a bad value is the default for that key, logged.
	void Load();

	// The rule an object has been given, if any: its base form's, else its
	// model's.
	[[nodiscard]] const Rule* Override(const Values& a_values, RE::TESObjectREFR* a_ref);

	// Listens for the MCM script's mod event and reloads on it.
	void Install();
}
