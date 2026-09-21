// Modern Camera Collision -- third-person camera collision for Skyrim SE/AE
// Copyright (C) 2026 DisizMod
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

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
