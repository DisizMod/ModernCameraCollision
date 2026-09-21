#include "Collision.h"
#include "Draw.h"
#include "Fade.h"
#include "Log.h"
#include "Settings.h"

namespace
{
	// SE 1.5.97 is where every hook was found and run; AE (1.6 and 1.7)
	// resolves the same functions through SE/AE id pairs. VR is refused:
	// its camera state and renderer layouts were never tried.
	bool IsSupportedRuntime()
	{
		return !REL::Module::IsVR();
	}

	// What L_CAMERA collides with, from the COLL records, so a layer rule
	// can be set knowing what is on the layer.
	void LogCameraLayers()
	{
		auto* data = RE::TESDataHandler::GetSingleton();
		if (!data) {
			return;
		}
		for (const auto* layer : data->GetFormArray<RE::BGSCollisionLayer>()) {
			if (!layer || layer->collisionIdx != static_cast<std::uint32_t>(RE::COL_LAYER::kCameraPick)) {
				continue;
			}
			std::string with;
			for (const auto* other : layer->collidesWith) {
				if (other) {
					with += fmt::format("{}{} ({})", with.empty() ? "" : ", ", other->name.c_str(), other->collisionIdx);
				}
			}
			spdlog::info("L_CAMERA collides with: {}", with);
		}
	}

	void OnMessage(SKSE::MessagingInterface::Message* a_message)
	{
		if (!a_message || a_message->type != SKSE::MessagingInterface::kDataLoaded) {
			return;
		}
		spdlog::info("data loaded");
		LogCameraLayers();
		mcc::settings::Load();
		mcc::settings::Install();
		if (!mcc::draw::Install()) {
			spdlog::warn("the debug drawing is unavailable; the fade will still release on the camera's own updates");
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	mcc::SetupLog();

	const auto version = REL::Module::get().version();
	spdlog::info("ModernCameraCollision " MCC_VERSION " loading against runtime {}.{}.{}.{}", version[0], version[1], version[2], version[3]);

	if (version[0] == 0 && version[1] == 0 && version[2] == 0) {
		spdlog::critical("runtime version read as 0.0.0.0: an address was resolved during static initialisation");
		return false;
	}
	if (!IsSupportedRuntime()) {
		spdlog::critical("UNSUPPORTED RUNTIME {}.{}.{}.{} -- ModernCameraCollision runs on Skyrim SE and AE, not VR, and is refusing to load",
			version[0], version[1], version[2], version[3]);
		return false;
	}
	spdlog::info("runtime is {}", REL::Module::IsAE() ? "AE" : "SE");

	SKSE::Init(a_skse);

	if (const auto* messaging = SKSE::GetMessagingInterface(); !messaging || !messaging->RegisterListener(OnMessage)) {
		spdlog::error("failed to register the messaging listener");
		return false;
	}
	if (!mcc::collision::Install()) {
		spdlog::critical("the collision hooks failed; nothing will happen");
		return false;
	}
	if (!mcc::fade::Install()) {
		spdlog::warn("the shadow-pass hook failed; faded shapes will not cast shadows");
	}

	spdlog::info("ModernCameraCollision loaded");
	return true;
}
