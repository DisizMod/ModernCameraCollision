#pragma once

#include <spdlog/sinks/basic_file_sink.h>

namespace mcc
{
	// Documents/My Games/Skyrim Special Edition/SKSE/ModernCameraCollision.log
	inline void SetupLog()
	{
		auto directory = SKSE::log::log_directory();
		if (!directory) {
			return;
		}
		*directory /= "ModernCameraCollision.log";

		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(directory->string(), true);
		auto log = std::make_shared<spdlog::logger>("mcc", std::move(sink));
		log->set_level(spdlog::level::info);
		log->flush_on(spdlog::level::info);
		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("[%H:%M:%S.%e] [%l] %v");
	}
}
