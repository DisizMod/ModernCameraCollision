#include "Settings.h"

// windows.h's min/max macros, already in through CommonLib's PCH, break
// SimpleIni's std::numeric_limits calls; taken off for its include.
#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include <SimpleIni.h>
#pragma pop_macro("max")
#pragma pop_macro("min")

namespace mcc::settings
{
	namespace
	{
		std::mutex g_mutex;
		Values     g_values;

		constexpr const char* kDefaults = "Data/MCM/Config/ModernCameraCollision/settings.ini";
		constexpr const char* kUser = "Data/MCM/Settings/ModernCameraCollision.ini";
		constexpr const char* kEvent = "MCC_SettingsChanged";

		// One file's keys applied over a_values; keys it lacks are left as
		// they are, which is how the user's file rides over the defaults.
		void Apply(const CSimpleIniA& a_ini, Values& a_values)
		{
			const auto b = [&](const char* section, const char* key, bool& out) {
				if (const char* v = a_ini.GetValue(section, key)) {
					out = std::atoi(v) != 0 || _stricmp(v, "true") == 0;
				}
			};
			const auto f = [&](const char* section, const char* key, float& out) {
				if (const char* v = a_ini.GetValue(section, key)) {
					out = static_cast<float>(std::atof(v));
				}
			};
			const auto i = [&](const char* section, const char* key, int& out) {
				if (const char* v = a_ini.GetValue(section, key)) {
					out = std::atoi(v);
				}
			};

			b("General", "bEnabled", a_values.enabled);

			f("Motion", "fHoldSecs", a_values.holdSecs);
			f("Motion", "fEaseInSecs", a_values.easeInSecs);
			f("Motion", "fEaseOutSecs", a_values.easeOutSecs);

			f("Rules", "fMinBound", a_values.minBound);
			f("Rules", "fDiscRadius", a_values.discRadius);
			b("Rules", "bDiscScales", a_values.discScales);
			f("Rules", "fDiscReference", a_values.discReference);
			f("Rules", "fDiscBehind", a_values.discBehind);
			i("Rules", "iDiscRings", a_values.discRings);
			i("Rules", "iDropBelowPercent", a_values.dropBelowPercent);
			f("Rules", "fPartDistance", a_values.partDistance);

			b("Whiskers", "bWhiskers", a_values.whiskers);
			b("Whiskers", "bVertical", a_values.whiskersVertical);
			b("Whiskers", "bDiagonal", a_values.whiskersDiagonal);
			b("Whiskers", "bShorten", a_values.shorten);
			b("Whiskers", "bSwing", a_values.swing);
			f("Whiskers", "fSwingYawDegPerSec", a_values.swingYawDegPerSec);
			f("Whiskers", "fSwingPitchDegPerSec", a_values.swingPitchDegPerSec);
			b("Whiskers", "bSwingYawInverted", a_values.swingYawInverted);
			b("Whiskers", "bSwingPitchInverted", a_values.swingPitchInverted);
			b("Whiskers", "bIntentWins", a_values.intentWins);

			b("Fade", "bFade", a_values.fade);
			f("Fade", "fAlpha", a_values.fadeAlpha);
			f("Fade", "fSecs", a_values.fadeSecs);
			b("Fade", "bFadeOwnAlpha", a_values.fadeOwnAlpha);
			b("Fade", "bKeepShadows", a_values.keepShadows);

			// [Layers]: every key is "i<layer name>", the value a Rule.
			CSimpleIniA::TNamesDepend keys;
			if (a_ini.GetAllKeys("Layers", keys)) {
				for (const auto& key : keys) {
					const std::string name = key.pItem;
					if (name.size() > 1 && name[0] == 'i') {
						const int rule = std::clamp(std::atoi(a_ini.GetValue("Layers", key.pItem, "0")), 0, 2);
						a_values.layerRules[name.substr(1)] = static_cast<Rule>(rule);
					}
				}
			}

			b("Debug", "bDrawDiscs", a_values.drawDiscs);
			b("Debug", "bDrawWhiskers", a_values.drawWhiskers);
			b("Debug", "bDrawRays", a_values.drawRays);
			b("Debug", "bDrawBounds", a_values.drawBounds);
			b("Debug", "bLogVerbose", a_values.logVerbose);
		}

		bool Read(const char* a_path, Values& a_values)
		{
			CSimpleIniA ini;
			ini.SetUnicode();
			if (ini.LoadFile(a_path) < 0) {
				return false;
			}
			Apply(ini, a_values);
			return true;
		}

		class EventSink final : public RE::BSTEventSink<SKSE::ModCallbackEvent>
		{
		public:
			static EventSink* Get()
			{
				static EventSink sink;
				return &sink;
			}

			RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event,
				RE::BSTEventSource<SKSE::ModCallbackEvent>*) override
			{
				if (a_event && a_event->eventName == kEvent) {
					spdlog::info("settings: the menu closed; reading again");
					Load();
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	Values Current()
	{
		std::lock_guard<std::mutex> lock(g_mutex);
		return g_values;
	}

	void Load()
	{
		Values     values;  // the built-in defaults
		const bool defaults = Read(kDefaults, values);
		const bool user = Read(kUser, values);

		// Kept sane whatever the files say.
		values.discRings = std::clamp(values.discRings, 1, 3);
		values.dropBelowPercent = std::clamp(values.dropBelowPercent, 0, 100);
		values.fadeAlpha = std::clamp(values.fadeAlpha, 0.0f, 1.0f);
		values.easeInSecs = (std::max)(values.easeInSecs, 0.005f);
		values.easeOutSecs = (std::max)(values.easeOutSecs, 0.01f);

		{
			std::lock_guard<std::mutex> lock(g_mutex);
			g_values = values;
		}
		spdlog::info("settings: defaults {}, user file {}; enabled {}, min bound {:.0f}, disc {:.0f} x {} ring(s) drop below {}%, "
					 "hold {:.2f}s ease in {:.3f}s out {:.2f}s, whiskers {} shorten {} swing {}, fade {} at {:.2f} over {:.2f}s, {} layer rule(s)",
			defaults ? "read" : "missing", user ? "read" : "missing", values.enabled, values.minBound, values.discRadius,
			values.discRings, values.dropBelowPercent, values.holdSecs, values.easeInSecs, values.easeOutSecs, values.whiskers,
			values.shorten, values.swing, values.fade, values.fadeAlpha, values.fadeSecs, values.layerRules.size());
	}

	void Install()
	{
		if (auto* source = SKSE::GetModCallbackEventSource()) {
			source->AddEventSink(EventSink::Get());
		}
	}
}
