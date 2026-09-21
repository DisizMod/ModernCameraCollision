#include "Settings.h"

#include <filesystem>

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
		constexpr const char* kOverrides = "Data/SKSE/Plugins/ModernCameraCollision/Overrides";

		std::string Lower(std::string a_s)
		{
			for (auto& c : a_s) {
				c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			}
			return a_s;
		}

		bool ParseRule(const char* a_value, Rule& a_rule)
		{
			const std::string v = Lower(a_value ? a_value : "");
			if (v == "measure" || v == "0") { a_rule = Rule::Measure; return true; }
			if (v == "stop" || v == "1") { a_rule = Rule::Stop; return true; }
			if (v == "through" || v == "2") { a_rule = Rule::Through; return true; }
			return false;
		}

		// One override file: a section per object, "[Plugin.esp|0xFORMID]"
		// or "[model:path\to\file.nif]", with "rule = measure|stop|through".
		void ReadOverrides(const std::filesystem::path& a_path, Values& a_values)
		{
			CSimpleIniA ini;
			ini.SetUnicode();
			if (ini.LoadFile(a_path.string().c_str()) < 0) {
				spdlog::warn("overrides: could not read {}", a_path.string());
				return;
			}
			auto* data = RE::TESDataHandler::GetSingleton();
			CSimpleIniA::TNamesDepend sections;
			ini.GetAllSections(sections);
			int count = 0;
			for (const auto& section : sections) {
				Rule rule;
				if (!ParseRule(ini.GetValue(section.pItem, "rule"), rule)) {
					spdlog::warn("overrides: {} [{}]: no rule, or not one of measure, stop, through", a_path.filename().string(), section.pItem);
					continue;
				}
				const std::string name = section.pItem;
				if (name.rfind("model:", 0) == 0) {
					a_values.modelOverrides[Lower(name.substr(6))] = rule;
					++count;
					continue;
				}
				const auto bar = name.find('|');
				if (bar == std::string::npos || !data) {
					spdlog::warn("overrides: {} [{}]: expected Plugin.esp|0xFORMID or model:path", a_path.filename().string(), section.pItem);
					continue;
				}
				const std::string plugin = name.substr(0, bar);
				const auto        local = static_cast<std::uint32_t>(std::strtoul(name.substr(bar + 1).c_str(), nullptr, 0));
				const auto        form = data->LookupFormID(local & 0x00FFFFFF, plugin);
				if (form == 0) {
					spdlog::warn("overrides: {} [{}]: no such form (plugin not loaded?)", a_path.filename().string(), section.pItem);
					continue;
				}
				a_values.formOverrides[form] = rule;
				++count;
			}
			spdlog::info("overrides: {} object(s) from {}", count, a_path.filename().string());
		}

		void ReadAllOverrides(Values& a_values)
		{
			std::error_code error;
			if (!std::filesystem::is_directory(kOverrides, error)) {
				return;
			}
			std::vector<std::filesystem::path> files;
			for (const auto& entry : std::filesystem::directory_iterator(kOverrides, error)) {
				if (entry.is_regular_file() && Lower(entry.path().extension().string()) == ".ini") {
					files.push_back(entry.path());
				}
			}
			std::sort(files.begin(), files.end());  // later names win
			for (const auto& file : files) {
				ReadOverrides(file, a_values);
			}
		}

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
			f("Rules", "fBodyHalfWidth", a_values.bodyHalfWidth);
			f("Rules", "fBodyAbove", a_values.bodyAbove);
			f("Rules", "fBodyBelow", a_values.bodyBelow);
			i("Rules", "iBodyColumns", a_values.bodyColumns);
			i("Rules", "iBodyRows", a_values.bodyRows);
			f("Rules", "fSideReach", a_values.sideReach);
			i("Rules", "iSidePoints", a_values.sidePoints);
			i("Rules", "iSideRows", a_values.sideRows);
			f("Rules", "fHighAngle", a_values.highAngle);
			f("Rules", "fHighScale", a_values.highScale);
			i("Rules", "iHighPoints", a_values.highPoints);
			i("Rules", "iDropBelowPercent", a_values.dropBelowPercent);
			f("Rules", "fPartDistance", a_values.partDistance);

			b("Prediction", "bPredictionRays", a_values.predictionRays);
			b("Prediction", "bVertical", a_values.predictionVertical);
			b("Prediction", "bDiagonal", a_values.predictionDiagonal);
			b("Prediction", "bShorten", a_values.shorten);
			b("Prediction", "bSwing", a_values.swing);
			f("Prediction", "fSwingYawDegPerSec", a_values.swingYawDegPerSec);
			f("Prediction", "fSwingPitchDegPerSec", a_values.swingPitchDegPerSec);
			b("Prediction", "bSwingYawInverted", a_values.swingYawInverted);
			b("Prediction", "bSwingPitchInverted", a_values.swingPitchInverted);
			b("Prediction", "bIntentWins", a_values.intentWins);

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
			b("Debug", "bDrawPredictionRays", a_values.drawPredictionRays);
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
		ReadAllOverrides(values);

		// Kept sane whatever the files say.
		values.bodyColumns = std::clamp(values.bodyColumns, 1, 7);
		values.bodyRows = std::clamp(values.bodyRows, 1, 9);
		values.sidePoints = std::clamp(values.sidePoints, 0, 6);
		values.sideRows = std::clamp(values.sideRows, 1, 9);
		values.highScale = std::clamp(values.highScale, 0.2f, 1.0f);
		values.highPoints = std::clamp(values.highPoints, 4, 16);
		values.dropBelowPercent = std::clamp(values.dropBelowPercent, 0, 100);
		values.fadeAlpha = std::clamp(values.fadeAlpha, 0.0f, 1.0f);
		values.easeInSecs = (std::max)(values.easeInSecs, 0.005f);
		values.easeOutSecs = (std::max)(values.easeOutSecs, 0.01f);

		{
			std::lock_guard<std::mutex> lock(g_mutex);
			g_values = values;
		}
		spdlog::info("settings: defaults {}, user file {}; enabled {}, min bound {:.0f}, body {:.0f} wide {:.0f} above {:.0f} below, {} x {} samples, "
					 "drop below {}%, same part {:.0f}, hold {:.2f}s ease in {:.3f}s out {:.2f}s, prediction rays {} shorten {} swing {}, "
					 "fade {} at {:.2f} over {:.2f}s, {} layer rule(s)",
			defaults ? "read" : "missing", user ? "read" : "missing", values.enabled, values.minBound, values.bodyHalfWidth * 2.0f,
			values.bodyAbove, values.bodyBelow, values.bodyColumns, values.bodyRows, values.dropBelowPercent, values.partDistance, values.holdSecs,
			values.easeInSecs, values.easeOutSecs, values.predictionRays, values.shorten, values.swing, values.fade, values.fadeAlpha,
			values.fadeSecs, values.layerRules.size());
		if (!values.formOverrides.empty() || !values.modelOverrides.empty()) {
			spdlog::info("settings: {} form override(s), {} model override(s)", values.formOverrides.size(), values.modelOverrides.size());
		}
	}

	const Rule* Override(const Values& a_values, RE::TESObjectREFR* a_ref)
	{
		if (!a_ref || (a_values.formOverrides.empty() && a_values.modelOverrides.empty())) {
			return nullptr;
		}
		const auto* base = a_ref->GetBaseObject();
		if (!base) {
			return nullptr;
		}
		if (const auto it = a_values.formOverrides.find(base->GetFormID()); it != a_values.formOverrides.end()) {
			return &it->second;
		}
		if (!a_values.modelOverrides.empty()) {
			if (const auto* model = base->As<RE::TESModel>()) {
				if (const auto it = a_values.modelOverrides.find(Lower(model->GetModel())); it != a_values.modelOverrides.end()) {
					return &it->second;
				}
			}
		}
		return nullptr;
	}

	void Install()
	{
		if (auto* source = SKSE::GetModCallbackEventSource()) {
			source->AddEventSink(EventSink::Get());
		}
	}
}
