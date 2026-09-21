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

// CommonLibSSE-NG's public headers are not self-contained: SKSE/Logger.h, for
// one, uses spdlog without including it and relies on SKSE/Impl/PCH.h having
// been pulled in first by SKSE/SKSE.h. Force-including this header on every
// translation unit is the configuration CommonLibSSE-NG's own consumer test
// uses.

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std::literals;

namespace mcc
{
	// A few vector helpers shared by every module.
	inline float Length(const RE::NiPoint3& a_v)
	{
		return std::sqrt(a_v.x * a_v.x + a_v.y * a_v.y + a_v.z * a_v.z);
	}

	inline RE::NiPoint3 Normalized(RE::NiPoint3 a_v)
	{
		const float len = Length(a_v);
		return len > 1e-6f ? a_v * (1.0f / len) : RE::NiPoint3{ 0.0f, 0.0f, 1.0f };
	}

	inline RE::NiPoint3 Cross(const RE::NiPoint3& a, const RE::NiPoint3& b)
	{
		return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
	}

	inline float X(const RE::hkVector4& a_v) { return a_v.quad.m128_f32[0]; }
	inline float Y(const RE::hkVector4& a_v) { return a_v.quad.m128_f32[1]; }
	inline float Z(const RE::hkVector4& a_v) { return a_v.quad.m128_f32[2]; }
	inline float W(const RE::hkVector4& a_v) { return a_v.quad.m128_f32[3]; }

	inline RE::NiPoint3 ToGame(const RE::hkVector4& a_v, float a_scale)
	{
		return { X(a_v) * a_scale, Y(a_v) * a_scale, Z(a_v) * a_scale };
	}

	inline RE::hkVector4 ToHavok(const RE::NiPoint3& a_p, float a_invScale)
	{
		return { a_p.x * a_invScale, a_p.y * a_invScale, a_p.z * a_invScale, 0.0f };
	}

	constexpr float kPi = 3.14159265f;
	constexpr float kDegToRad = kPi / 180.0f;
}
