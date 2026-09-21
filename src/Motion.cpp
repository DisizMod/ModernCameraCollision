#include "Motion.h"

#include "Collision.h"
#include "Prediction.h"

namespace mcc::motion
{
	namespace
	{
		float                                 g_pull = 1.0f;  // the fraction the camera is held at
		std::chrono::steady_clock::time_point g_holdUntil;
		std::chrono::steady_clock::time_point g_last = std::chrono::steady_clock::now();
	}

	void Apply(RE::ThirdPersonState* a_state, bool a_castThisUpdate, const settings::Values& a_settings)
	{
		const auto  now = std::chrono::steady_clock::now();
		const float dt = (std::min)(std::chrono::duration<float>(now - g_last).count(), 0.1f);
		g_last = now;
		if (!a_castThisUpdate) {
			g_pull = 1.0f;
			return;
		}

		const RE::NiPoint3& from = collision::CastFrom();
		const RE::NiPoint3  span = collision::CastTo() - from;
		const float         length = Length(span);
		if (length < 1.0f) {
			return;
		}

		// Where the engine put the camera, as a fraction of the cast, and
		// the nearer of that and the prediction rays' cap.
		const float wanted = std::clamp(Length(a_state->translation - from) / length, 0.0f, 1.0f);
		const float cap = a_settings.shorten ? prediction::NearFree() : 1.0f;
		const float target = (std::min)(wanted, cap);

		if (target < g_pull) {
			// The way in: eased. While it eases the camera is farther out
			// than the engine's stop -- a few units short of the surface --
			// so the constant is kept small.
			g_pull += (target - g_pull) * (1.0f - std::exp(-dt / (std::max)(a_settings.easeInSecs, 0.005f)));
			if (g_pull - target < 0.001f) {
				g_pull = target;
			}
			g_holdUntil = now + std::chrono::milliseconds(static_cast<int>(a_settings.holdSecs * 1000.0f));
		} else if (now >= g_holdUntil) {
			// The way out, after the hold.
			g_pull += (target - g_pull) * (1.0f - std::exp(-dt / (std::max)(a_settings.easeOutSecs, 0.01f)));
			if (target - g_pull < 0.001f) {
				g_pull = target;
			}
		}

		if (std::fabs(g_pull - wanted) > 0.0005f) {
			a_state->translation = from + span * g_pull;
			a_state->collisionPos = a_state->translation;
		}
		prediction::Swing(a_state, dt, a_settings);
	}

	float Pull()
	{
		return g_pull;
	}
}
