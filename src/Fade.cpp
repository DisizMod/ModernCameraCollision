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

#include "Fade.h"

namespace mcc::fade
{
	namespace
	{
		constexpr float kUnderSlack = 10.0f;  // the collision mesh sits a little off the visible one

		struct Faded
		{
			RE::NiPointer<RE::BSGeometry>      geometry;
			RE::NiPointer<RE::NiAlphaProperty> previousAlpha;
			bool                               ownProperty;  // ours is on the shape; the previous goes back at the end
			float                              previous;     // the material alpha it had
			float                              current;
			bool                               seen;
			bool                               releasing;
		};

		std::unordered_map<RE::BSGeometry*, Faded> g_faded;  // main thread only
		std::chrono::steady_clock::time_point      g_lastStep = std::chrono::steady_clock::now();

		// The shadow pass drops a geometry whose alpha property blends, so a
		// faded wall stops casting. GetRenderPasses_ShadowMapOrMask is
		// hooked and, for a shape whose blend property is ours, shown the
		// property the shape had before for the length of the call. In a
		// map of its own under a lock, since that pass runs off the game's
		// thread.
		constexpr std::size_t kShadowPassesIndex = 0x2B;
		using ShadowPassesFn = RE::BSShaderProperty::RenderPassArray*(RE::BSLightingShaderProperty*, RE::BSGeometry*, std::uint32_t, RE::BSShaderAccumulator*);
		ShadowPassesFn* g_originalShadowPasses = nullptr;

		std::mutex                                                             g_shadowMutex;
		std::unordered_map<RE::BSGeometry*, RE::NiPointer<RE::NiAlphaProperty>> g_shadowPrevious;
		std::atomic<bool>                                                      g_keepShadows{ true };

		RE::BSShaderProperty::RenderPassArray* ShadowPassesHook(RE::BSLightingShaderProperty* a_this, RE::BSGeometry* a_geometry,
			std::uint32_t a_renderMode, RE::BSShaderAccumulator* a_accumulator)
		{
			if (!g_keepShadows.load(std::memory_order_relaxed) || !a_geometry) {
				return g_originalShadowPasses(a_this, a_geometry, a_renderMode, a_accumulator);
			}
			RE::NiPointer<RE::NiAlphaProperty> previous;
			bool                               swap = false;
			{
				std::lock_guard<std::mutex> lock(g_shadowMutex);
				if (const auto it = g_shadowPrevious.find(a_geometry); it != g_shadowPrevious.end()) {
					previous = it->second;
					swap = true;
				}
			}
			if (!swap) {
				return g_originalShadowPasses(a_this, a_geometry, a_renderMode, a_accumulator);
			}
			auto& runtime = a_geometry->GetGeometryRuntimeData();
			auto  ours = runtime.alphaProperty;
			runtime.alphaProperty = previous;
			auto* passes = g_originalShadowPasses(a_this, a_geometry, a_renderMode, a_accumulator);
			runtime.alphaProperty = ours;
			return passes;
		}

		// An alpha property made from nothing: blend on, source alpha over
		// inverse source alpha, no test.
		RE::NiAlphaProperty* MakeBlendAlpha()
		{
			auto* alpha = static_cast<RE::NiAlphaProperty*>(RE::malloc(sizeof(RE::NiAlphaProperty)));
			if (!alpha) {
				return nullptr;
			}
			std::memset(static_cast<void*>(alpha), 0, sizeof(RE::NiAlphaProperty));
			if (!SKSE::stl::emplace_vtable(alpha)) {
				RE::free(alpha);
				return nullptr;
			}
			using F = RE::NiAlphaProperty::AlphaFunction;
			alpha->alphaFlags = static_cast<std::uint16_t>(1 | (static_cast<int>(F::kSrcAlpha) << 1) | (static_cast<int>(F::kInvSrcAlpha) << 5));
			alpha->alphaThreshold = 0;
			return alpha;
		}

		RE::BSLightingShaderProperty* LightingOf(RE::BSGeometry* a_geometry)
		{
			auto* shader = a_geometry->GetGeometryRuntimeData().shaderProperty.get();
			return shader ? netimmerse_cast<RE::BSLightingShaderProperty*>(shader) : nullptr;
		}

		void WriteAlpha(Faded& a_faded, float a_alpha)
		{
			auto* lighting = LightingOf(a_faded.geometry.get());
			if (auto* material = lighting ? static_cast<RE::BSLightingShaderMaterialBase*>(lighting->material) : nullptr) {
				material->materialAlpha = a_alpha;
			}
		}

		void Begin(RE::BSGeometry* a_geometry)
		{
			if (auto it = g_faded.find(a_geometry); it != g_faded.end()) {
				it->second.seen = true;
				return;
			}
			auto* lighting = LightingOf(a_geometry);
			if (!lighting || !lighting->material) {
				return;
			}
			// A material of this shape's own, copied from the shared one.
			lighting->SetMaterial(lighting->material, true);
			auto* material = static_cast<RE::BSLightingShaderMaterialBase*>(lighting->material);
			if (!material) {
				return;
			}
			auto& runtime = a_geometry->GetGeometryRuntimeData();
			Faded faded{ RE::NiPointer<RE::BSGeometry>{ a_geometry }, runtime.alphaProperty, false, material->materialAlpha,
				material->materialAlpha, true, false };
			// A shape with an alpha property already keeps it.
			if (!runtime.alphaProperty) {
				if (auto* alpha = MakeBlendAlpha()) {
					runtime.alphaProperty.reset(alpha);
					faded.ownProperty = true;
					std::lock_guard<std::mutex> lock(g_shadowMutex);
					g_shadowPrevious[a_geometry] = faded.previousAlpha;
				}
			}
			g_faded.emplace(a_geometry, std::move(faded));
		}

		void Step(const settings::Values& a_settings)
		{
			const auto  now = std::chrono::steady_clock::now();
			const float dt = std::chrono::duration<float>(now - g_lastStep).count();
			g_lastStep = now;
			const float rate = (1.0f - a_settings.fadeAlpha) / (std::max)(a_settings.fadeSecs, 0.01f) * (std::min)(dt, 0.1f);

			for (auto it = g_faded.begin(); it != g_faded.end();) {
				auto&       faded = it->second;
				const float target = faded.releasing ? faded.previous : a_settings.fadeAlpha;
				if (faded.current < target) {
					faded.current = (std::min)(faded.current + rate, target);
				} else {
					faded.current = (std::max)(faded.current - rate, target);
				}
				WriteAlpha(faded, faded.current);
				if (faded.releasing && faded.current == target) {
					if (faded.ownProperty) {
						{
							std::lock_guard<std::mutex> lock(g_shadowMutex);
							g_shadowPrevious.erase(faded.geometry.get());
						}
						faded.geometry->GetGeometryRuntimeData().alphaProperty = faded.previousAlpha;
					}
					it = g_faded.erase(it);
				} else {
					++it;
				}
			}
		}

		void ReleaseUnseen()
		{
			for (auto& [geometry, faded] : g_faded) {
				faded.releasing = !faded.seen;
				faded.seen = false;
			}
		}

		std::atomic<std::uint32_t> g_updates{ 0 };
		std::atomic<std::uint32_t> g_updatesAtTick{ 0 };
	}

	bool Install()
	{
		REL::Relocation<std::uintptr_t> vtable{ RE::VTABLE_BSLightingShaderProperty[0] };
		g_originalShadowPasses = reinterpret_cast<ShadowPassesFn*>(
			*reinterpret_cast<std::uintptr_t*>(vtable.address() + sizeof(void*) * kShadowPassesIndex));
		if (!g_originalShadowPasses) {
			spdlog::error("fade: BSLightingShaderProperty's vtable holds no shadow passes function");
			return false;
		}
		vtable.write_vfunc(kShadowPassesIndex, ShadowPassesHook);
		spdlog::info("fade: hooked BSLightingShaderProperty::GetRenderPasses_ShadowMapOrMask");
		return true;
	}

	bool WouldTake(RE::BSGeometry* a_geometry, const RE::NiPoint3& a_at, const settings::Values& a_settings,
		float& a_toBound, const char*& a_why)
	{
		const auto& bound = a_geometry->worldBound;
		a_toBound = Length(bound.center - a_at) - bound.radius;
		if (a_toBound > kUnderSlack) {
			a_why = "not under the hit";
			return false;
		}
		if (bound.radius >= a_settings.minBound) {
			a_why = "bound at or above the minimum";
			return false;
		}
		const bool ownAlpha = a_geometry->GetGeometryRuntimeData().alphaProperty != nullptr;
		if (ownAlpha && !a_settings.fadeOwnAlpha && g_faded.find(a_geometry) == g_faded.end()) {
			a_why = "has its own alpha";
			return false;
		}
		a_why = "small";
		return true;
	}

	bool Fadeable(RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_at, const settings::Values& a_settings)
	{
		auto* root = a_ref ? a_ref->Get3D() : nullptr;
		if (!root) {
			return false;
		}
		bool any = false;
		RE::BSVisit::TraverseScenegraphGeometries(root, [&](RE::BSGeometry* a_geometry) {
			float       toBound;
			const char* why;
			if (WouldTake(a_geometry, a_at, a_settings, toBound, why)) {
				any = true;
				return RE::BSVisit::BSVisitControl::kStop;
			}
			return RE::BSVisit::BSVisitControl::kContinue;
		});
		return any;
	}

	void Around(RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_at, bool a_whole, const settings::Values& a_settings)
	{
		if (!a_settings.fade) {
			return;
		}
		auto* root = a_ref ? a_ref->Get3D() : nullptr;
		if (!root) {
			return;
		}
		RE::BSVisit::TraverseScenegraphGeometries(root, [&](RE::BSGeometry* a_geometry) {
			float       toBound;
			const char* why;
			const bool  takes = WouldTake(a_geometry, a_at, a_settings, toBound, why);
			// Faded whole by its layer: every shape that can be faded at all.
			const bool ownAlpha = a_geometry->GetGeometryRuntimeData().alphaProperty != nullptr;
			const bool wholeTakes = a_whole && (!ownAlpha || a_settings.fadeOwnAlpha || g_faded.contains(a_geometry));
			if (takes || wholeTakes) {
				Begin(a_geometry);
			}
			return RE::BSVisit::BSVisitControl::kContinue;
		});
	}

	void Update(const settings::Values& a_settings)
	{
		g_updates.fetch_add(1, std::memory_order_relaxed);
		g_keepShadows.store(a_settings.keepShadows, std::memory_order_relaxed);
		ReleaseUnseen();
		Step(a_settings);
	}

	void Tick()
	{
		const auto updates = g_updates.load(std::memory_order_relaxed);
		const auto before = g_updatesAtTick.exchange(updates, std::memory_order_relaxed);
		if (updates != before) {
			return;
		}
		if (auto* tasks = SKSE::GetTaskInterface()) {
			tasks->AddTask([]() {
				if (!g_faded.empty() && g_updates.load(std::memory_order_relaxed) == g_updatesAtTick.load(std::memory_order_relaxed)) {
					for (auto& [geometry, faded] : g_faded) {
						faded.seen = false;
					}
					ReleaseUnseen();
					Step(settings::Current());
				}
			});
		}
	}

	std::vector<std::pair<RE::NiPoint3, float>> Bounds()
	{
		std::vector<std::pair<RE::NiPoint3, float>> out;
		for (const auto& [geometry, faded] : g_faded) {
			out.push_back({ geometry->worldBound.center, geometry->worldBound.radius });
		}
		return out;
	}
}
