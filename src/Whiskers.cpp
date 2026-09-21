#include "Whiskers.h"

#include "Collision.h"

namespace mcc::whiskers
{
	namespace
	{
		enum class Group : std::uint8_t
		{
			Horizontal,
			Vertical,
			Diagonal,
			Behind,
		};

		struct Def
		{
			float yaw;    // degrees off the camera's line; positive is counter-clockwise from above
			float pitch;  // degrees; positive rises with the camera's up
			float weight; // nearer the camera's line counts more
			Group group;
			const char* name;
		};

		constexpr Def kDefs[] = {
			{ -30.0f, 0.0f, 0.3f, Group::Horizontal, "yaw -30" },
			{ -20.0f, 0.0f, 0.55f, Group::Horizontal, "yaw -20" },
			{ -10.0f, 0.0f, 0.8f, Group::Horizontal, "yaw -10" },
			{ 10.0f, 0.0f, 0.8f, Group::Horizontal, "yaw +10" },
			{ 20.0f, 0.0f, 0.55f, Group::Horizontal, "yaw +20" },
			{ 30.0f, 0.0f, 0.3f, Group::Horizontal, "yaw +30" },
			{ 0.0f, -30.0f, 0.3f, Group::Vertical, "pitch -30" },
			{ 0.0f, -20.0f, 0.55f, Group::Vertical, "pitch -20" },
			{ 0.0f, -10.0f, 0.8f, Group::Vertical, "pitch -10" },
			{ 0.0f, 10.0f, 0.8f, Group::Vertical, "pitch +10" },
			{ 0.0f, 20.0f, 0.55f, Group::Vertical, "pitch +20" },
			{ 0.0f, 30.0f, 0.3f, Group::Vertical, "pitch +30" },
			{ -20.0f, -20.0f, 0.55f, Group::Diagonal, "yaw -20 pitch -20" },
			{ 20.0f, -20.0f, 0.55f, Group::Diagonal, "yaw +20 pitch -20" },
			{ -20.0f, 20.0f, 0.55f, Group::Diagonal, "yaw -20 pitch +20" },
			{ 20.0f, 20.0f, 0.55f, Group::Diagonal, "yaw +20 pitch +20" },
			{ 0.0f, 0.0f, 0.0f, Group::Behind, "behind" },
		};
		constexpr float kBehindUnits = 80.0f;
		constexpr float kBodyClearance = 40.0f;  // a ray from the player starts this far out, clear of the body

		float g_blockedLeft = 0.0f;   // 0..1, this update: how blocked each side is
		float g_blockedRight = 0.0f;
		float g_blockedDown = 0.0f;
		float g_blockedUp = 0.0f;
		float g_nearFree = 1.0f;

		std::chrono::steady_clock::time_point g_lastLookInput;

		// A whisker's fraction of the whole a_from..a_to: 1 when clear, and 1
		// as well when what it found is something the camera would be let
		// through. The ray itself starts clear of the player's capsule.
		float Fraction(const RE::hkpWorld* a_world, const RE::NiPoint3& a_from, const RE::NiPoint3& a_to, float a_scale,
			const settings::Values& a_settings, const char* a_name)
		{
			const float        length = Length(a_to - a_from);
			const float        clearance = (std::min)(kBodyClearance, length * 0.5f);
			const RE::NiPoint3 start = a_from + Normalized(a_to - a_from) * clearance;

			RE::hkpWorldRayCastOutput output;
			collision::CastRay(a_world, start, a_to, a_scale, output);
			if (!output.HasHit()) {
				collision::Record(collision::RayKind::Whisker, start, a_to, output, false, false);
				return 1.0f;
			}
			const RE::NiPoint3 at = start + (a_to - start) * output.hitFraction;
			const float        fraction = length > 0.0f ? (clearance + output.hitFraction * (length - clearance)) / length : 1.0f;
			auto*              ref = output.rootCollidable ? RE::TESHavokUtilities::FindCollidableRef(*output.rootCollidable) : nullptr;

			// Judged with a disc of its own, from the whisker's far end: where
			// the camera would be at its angle.
			const auto verdict = collision::Judge(a_world, output.rootCollidable, ref, at, a_to, a_scale, true, a_settings);
			const bool stops = verdict.stops;
			if (collision::Verbose()) {
				spdlog::info("  whisker {} at {:.2f} cover {}% {}{}{} {}", a_name, fraction, verdict.cover,
					verdict.whole ? "through by layer " : "", verdict.isSmall ? "small " : "", stops ? "COUNTS" : "let through",
					collision::Describe(output.rootCollidable));
			}
			collision::Record(collision::RayKind::Whisker, start, a_to, output, stops, false);
			return stops ? fraction : 1.0f;
		}
	}

	void Cast(const RE::hkpWorld* a_world, float a_scale, const settings::Values& a_settings)
	{
		g_blockedLeft = g_blockedRight = g_blockedDown = g_blockedUp = 0.0f;
		g_nearFree = 1.0f;
		if (!a_settings.whiskers) {
			return;
		}

		// The camera's line and the two axes across it.
		const RE::NiPoint3& from = collision::CastFrom();
		const RE::NiPoint3& to = collision::CastTo();
		const RE::NiPoint3  span = to - from;
		const float         length = Length(span);
		const RE::NiPoint3  dir = Normalized(span);
		RE::NiPoint3        worldUp{ 0.0f, 0.0f, 1.0f };
		if (std::fabs(dir.z) > 0.9f) {
			worldUp = { 1.0f, 0.0f, 0.0f };
		}
		const RE::NiPoint3 right = Normalized(Cross(dir, worldUp));
		const RE::NiPoint3 up = Cross(right, dir);

		for (const auto& def : kDefs) {
			if (def.group == Group::Behind) {
				Fraction(a_world, to, to + dir * kBehindUnits, a_scale, a_settings, def.name);
				continue;
			}
			if ((def.group == Group::Vertical && !a_settings.whiskersVertical) ||
				(def.group == Group::Diagonal && !a_settings.whiskersDiagonal)) {
				continue;
			}
			const float        y = def.yaw * kDegToRad;
			const float        p = def.pitch * kDegToRad;
			const RE::NiPoint3 turned = (dir * (std::cos(p) * std::cos(y)) - right * (std::cos(p) * std::sin(y)) + up * std::sin(p)) * length;
			const float        fraction = Fraction(a_world, from, from + turned, a_scale, a_settings, def.name);

			// How blocked a side is: the most blocked of its whiskers.
			const float blocked = def.weight * (1.0f - fraction);
			if (def.yaw < 0.0f) {
				g_blockedLeft = (std::max)(g_blockedLeft, blocked);
			} else if (def.yaw > 0.0f) {
				g_blockedRight = (std::max)(g_blockedRight, blocked);
			}
			if (def.pitch < 0.0f) {
				g_blockedDown = (std::max)(g_blockedDown, blocked);
			} else if (def.pitch > 0.0f) {
				g_blockedUp = (std::max)(g_blockedUp, blocked);
			}
			// The near ones measure what is free in about the camera's direction.
			if (std::fabs(def.yaw) <= 10.0f && std::fabs(def.pitch) <= 10.0f) {
				g_nearFree = (std::min)(g_nearFree, fraction);
			}
		}
	}

	float NearFree()
	{
		return g_nearFree;
	}

	void Swing(RE::ThirdPersonState* a_state, float a_dt, const settings::Values& a_settings)
	{
		if (!a_settings.whiskers || !a_settings.swing) {
			return;
		}
		const auto now = std::chrono::steady_clock::now();
		if (auto* controls = RE::PlayerControls::GetSingleton()) {
			const auto& look = controls->data.lookInputVec;
			if (std::fabs(look.x) > 0.0f || std::fabs(look.y) > 0.0f) {
				g_lastLookInput = now;
			}
		}
		if (a_settings.intentWins && std::chrono::duration<float>(now - g_lastLookInput).count() < 0.25f) {
			return;
		}
		const float drive = g_blockedLeft - g_blockedRight;
		if (std::fabs(drive) >= 0.01f) {
			float rate = drive * a_settings.swingYawDegPerSec;
			if (a_settings.swingYawInverted) {
				rate = -rate;
			}
			a_state->freeRotation.x += rate * kDegToRad * a_dt;
		}
		const float pitchDrive = g_blockedDown - g_blockedUp;
		if (std::fabs(pitchDrive) >= 0.01f) {
			float rate = pitchDrive * a_settings.swingPitchDegPerSec;
			if (a_settings.swingPitchInverted) {
				rate = -rate;
			}
			a_state->freeRotation.y += rate * kDegToRad * a_dt;
		}
	}
}
