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
		Through = 2,  // never stops it; faded whole
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
		float discRadius = 30.0f;    // the disc at a hit, facing the player
		bool  discScales = false;    // ... scaled by the camera's distance over the reference
		float discReference = 150.0f;
		float discBehind = 40.0f;    // the disc's rays end this far past it
		int   discRings = 2;         // sample rings of eight, plus the centre
		int   dropBelowPercent = 75; // dropped when the occluder takes less of the disc than this
		float partDistance = 80.0f;  // a hit on the occluder counts only within this distance of the sweep's hit: the same part of it

		// [Whiskers]
		bool  whiskers = true;
		bool  whiskersVertical = true;
		bool  whiskersDiagonal = true;
		bool  shorten = true;          // the near whiskers cap the distance at what they measure free
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

		// [Debug]
		bool drawDiscs = false;
		bool drawWhiskers = false;
		bool drawRays = false;
		bool drawBounds = false;
		bool logVerbose = false;  // one update in sixty, and the first ten, in full
	};

	// A copy of what is set right now. Cheap; taken once per camera update.
	[[nodiscard]] Values Current();

	// Reads the two files. Missing files are the defaults; a bad value is
	// the default for that key, logged.
	void Load();

	// Listens for the MCM script's mod event and reloads on it.
	void Install();
}
