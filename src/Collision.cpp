#include "Collision.h"

#include "Fade.h"
#include "Motion.h"
#include "Whiskers.h"

#include <MinHook.h>

namespace mcc::collision
{
	namespace
	{
		constexpr std::uint64_t kUpdateCameraCollisionID = 49980;  // ThirdPersonState::UpdateCameraCollision, SE
		constexpr std::uint64_t kLinearCastID = 60554;             // hkpWorld::LinearCast, SE
		constexpr std::uint64_t kCastRayID = 60551;                // hkpWorld::CastRay, SE

		using UpdateCameraCollisionFn = void(RE::ThirdPersonState*);
		using LinearCastFn = void(const RE::hkpWorld*, const RE::hkpCollidable*, const RE::hkpLinearCastInput&,
			RE::hkpCdPointCollector&, RE::hkpCdPointCollector*);
		using CastRayFn = void(const RE::hkpWorld*, const RE::hkpWorldRayCastInput&, RE::hkpWorldRayCastOutput&);

		UpdateCameraCollisionFn* g_originalUpdate = nullptr;
		LinearCastFn*            g_originalLinearCast = nullptr;

		// The engine's ray cast, called as is; taken inside a function so no
		// address is resolved during static initialisation.
		CastRayFn* EngineCastRay()
		{
			static REL::Relocation<CastRayFn> fn{ REL::ID(kCastRayID) };
			return fn.get();
		}

		// The camera's collision runs on one thread; casts from other threads
		// (AI, projectiles) must not be mistaken for it.
		thread_local bool g_inCameraCollision = false;

		std::atomic<std::uint32_t> g_updates{ 0 };
		bool                       g_verbose = false;
		settings::Values           g_settings;  // this update's copy

		// This update's cast, in game units, set as the engine's sweep starts.
		bool         g_castThisUpdate = false;
		RE::NiPoint3 g_castFrom;
		RE::NiPoint3 g_castTo;
		RE::CFilter  g_castFilter;
		float        g_castLength = 0.0f;

		// --- what was cast --------------------------------------------------------
		std::mutex g_frameMutex;
		Frame      g_frame;     // the last update's, read by the render side
		Frame      g_building;  // this update's

		// --- names ------------------------------------------------------------------
		std::unordered_map<std::uint32_t, std::string> g_layerNames;
		std::once_flag                                 g_layerNamesOnce;

		const std::string& LayerName(RE::COL_LAYER a_layer)
		{
			std::call_once(g_layerNamesOnce, []() {
				if (auto* data = RE::TESDataHandler::GetSingleton()) {
					for (const auto* layer : data->GetFormArray<RE::BGSCollisionLayer>()) {
						if (layer) {
							g_layerNames[layer->collisionIdx] = layer->name.c_str();
						}
					}
				}
			});
			static const std::string unknown = "L_?";
			const auto                it = g_layerNames.find(static_cast<std::uint32_t>(a_layer));
			return it != g_layerNames.end() ? it->second : unknown;
		}

		std::string Describe(const RE::hkpCollidable* a_collidable)
		{
			if (!a_collidable) {
				return "null";
			}
			std::string out = LayerName(a_collidable->GetCollisionLayer());
			if (auto* ref = RE::TESHavokUtilities::FindCollidableRef(*a_collidable)) {
				out += fmt::format(" ref {:08X}", ref->GetFormID());
				if (const auto* base = ref->GetBaseObject()) {
					out += fmt::format(" [{}]", RE::FormTypeToString(base->GetFormType()));
					if (const auto* model = base->As<RE::TESModel>()) {
						out += std::string{ " " } + model->GetModel();
					}
				}
			}
			return out;
		}

		const RE::hkpCollidable* RootOf(const RE::hkpCdBody* a_body)
		{
			while (a_body && a_body->parent) {
				a_body = a_body->parent;
			}
			return static_cast<const RE::hkpCollidable*>(a_body);
		}

		// --- the layer's rule ---------------------------------------------------------
		settings::Rule RuleFor(RE::COL_LAYER a_layer)
		{
			if (const auto it = g_settings.layerRules.find(LayerName(a_layer)); it != g_settings.layerRules.end()) {
				return it->second;
			}
			switch (a_layer) {
			case RE::COL_LAYER::kTrees: return settings::Rule::Through;
			case RE::COL_LAYER::kTerrain:
			case RE::COL_LAYER::kGround: return settings::Rule::Stop;
			default: return settings::Rule::Measure;
			}
		}

		// --- the decision's memory ------------------------------------------------------
		//
		// The last decision per reference, so a cover wobbling about the
		// threshold does not flip the camera between two distances: once
		// dropped, an occluder needs the threshold plus twelve points to be
		// kept again; once kept, twenty-five under it to be dropped.
		// Forgotten half a second after it was last hit.
		constexpr float kDecisionSecs = 0.5f;

		struct Decision
		{
			bool                                  dropped;
			std::chrono::steady_clock::time_point at;
		};
		std::unordered_map<std::uint32_t, Decision> g_decisions;

		bool Decide(RE::TESObjectREFR* a_ref, int a_coverPercent)
		{
			const auto now = std::chrono::steady_clock::now();
			const auto id = a_ref ? a_ref->GetFormID() : 0u;
			const auto it = g_decisions.find(id);
			const int  keep = g_settings.dropBelowPercent;
			bool       drop;
			if (it == g_decisions.end()) {
				drop = a_coverPercent < keep;
			} else if (it->second.dropped) {
				drop = a_coverPercent < (std::min)(keep + 12, 100);
			} else {
				drop = a_coverPercent <= (std::max)(keep - 25, 0);
			}
			g_decisions[id] = { drop, now };
			return drop;
		}

		void ForgetOldDecisions()
		{
			const auto now = std::chrono::steady_clock::now();
			for (auto it = g_decisions.begin(); it != g_decisions.end();) {
				if (std::chrono::duration<float>(now - it->second.at).count() > kDecisionSecs) {
					it = g_decisions.erase(it);
				} else {
					++it;
				}
			}
		}

		// --- the disc -----------------------------------------------------------------
		//
		// How much of the player the camera could see past the occluder, in
		// percent of the samples: the centre and rings of eight points on a
		// disc about the pivot -- the player's extent -- facing the wanted
		// camera position, out to the disc radius. Each sample is reached by
		// a ray from the wanted camera position; the sample is blocked when
		// the occluder is on that line before the player, seen when the line
		// reaches the player with nothing solid on it.

		// A point of the body's stadium at angle a_angle, scaled by a_scale
		// toward the centre: half-width w across, the straight sides run
		// from -below+w to above-w, with half-circles of radius w capping
		// them. Returned in the plane's (across, up) coordinates.
		void StadiumPoint(float a_angle, float a_scale, float& a_x, float& a_y)
		{
			const float w = g_settings.bodyHalfWidth * a_scale;
			const float above = g_settings.bodyAbove * a_scale;
			const float below = g_settings.bodyBelow * a_scale;
			const float c = std::cos(a_angle), s = std::sin(a_angle);
			// The cap's centre: up for the upper half, down for the lower.
			const float capY = s >= 0.0f ? (std::max)(above - w, 0.0f) : -(std::max)(below - w, 0.0f);
			a_x = w * c;
			a_y = capY + w * s;
		}

		// Whether a sample ray may go on past what it hit: the player's own
		// body, a through layer, or a shape that is small at that point.
		bool SeesThrough(const RE::hkpCollidable* a_root, RE::TESObjectREFR* a_ref, const RE::NiPoint3& a_at)
		{
			const auto layer = a_root->GetCollisionLayer();
			if (layer == RE::COL_LAYER::kCharController) {
				return true;
			}
			const bool ground = layer == RE::COL_LAYER::kTerrain || layer == RE::COL_LAYER::kGround ||
			                    (a_root->shape && a_root->shape->type == RE::hkpShapeType::kHeightField);
			const auto rule = ground ? settings::Rule::Stop : RuleFor(layer);
			if (rule == settings::Rule::Through) {
				return true;
			}
			if (rule == settings::Rule::Stop) {
				return false;
			}
			return fade::Fadeable(a_ref, a_at, g_settings);
		}

		// Every hit along a ray, nearest first, through the engine's own pick
		// with an all-hits collector. The world is the player's cell's; the
		// pick takes the physics lock itself.
		struct Hit
		{
			float                    fraction;
			const RE::hkpCollidable* root;
		};

		void AllHits(const RE::NiPoint3& a_from, const RE::NiPoint3& a_to, float a_scale, std::vector<Hit>& a_out)
		{
			a_out.clear();
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* cell = player ? player->GetParentCell() : nullptr;
			auto* world = cell ? cell->GetbhkWorld() : nullptr;
			if (!world) {
				return;
			}
			RE::hkpAllRayHitTempCollector collector;
			RE::bhkPickData               pick;
			pick.rayInput.from = ToHavok(a_from, 1.0f / a_scale);
			pick.rayInput.to = ToHavok(a_to, 1.0f / a_scale);
			pick.rayInput.filterInfo = g_castFilter;
			pick.rayInput.enableShapeCollectionFilter = false;
			pick.allRayHitTempCollector = &collector;
			world->PickObject(pick);
			for (const auto& hit : collector.hits) {
				if (hit.rootCollidable) {
					a_out.push_back({ hit.hitFraction, hit.rootCollidable });
				}
			}
			std::sort(a_out.begin(), a_out.end(), [](const Hit& a, const Hit& b) { return a.fraction < b.fraction; });
		}

		int DiscCover(const RE::hkpWorld* a_world, const RE::NiPoint3& a_at, const RE::NiPoint3& a_eye, RE::TESObjectREFR* a_ref,
			float a_scale, bool a_forWhisker, DiscView& a_disc)
		{
			// The disc is the player, seen from where the camera would be.
			const RE::NiPoint3 eye = a_eye;
			const RE::NiPoint3 centre = g_castFrom;
			const RE::NiPoint3 axis = Normalized(centre - eye);
			RE::NiPoint3       up{ 0.0f, 0.0f, 1.0f };
			if (std::fabs(axis.z) > 0.9f) {
				up = { 1.0f, 0.0f, 0.0f };
			}
			// Across the body, and up it, in the plane facing the eye.
			const RE::NiPoint3 u = Normalized(Cross(axis, up));
			const RE::NiPoint3 v = Normalized(Cross(u, axis));
			const int          rings = std::clamp(g_settings.discRings, 1, 3);
			const auto         id = a_ref ? a_ref->GetFormID() : 0u;
			a_disc = DiscView{ centre, {}, false, a_forWhisker, 0, {}, {} };

			// The outline, for drawing; the samples on it and on smaller
			// copies inside, plus the centre.
			for (int i = 0; i < 32; ++i) {
				float x, y;
				StadiumPoint(static_cast<float>(i) * (2.0f * kPi / 32.0f), 1.0f, x, y);
				a_disc.outline[i] = centre + u * x + v * y;
			}
			int n = 0;
			a_disc.point[n++] = centre;
			for (int r = 1; r <= rings; ++r) {
				const float scale = static_cast<float>(r) / static_cast<float>(rings);
				for (int i = 0; i < 8; ++i) {
					const float angle = static_cast<float>(i) * (kPi / 4.0f) + (r % 2 ? 0.0f : kPi / 8.0f);
					float       x, y;
					StadiumPoint(angle, scale, x, y);
					a_disc.point[n++] = centre + u * x + v * y;
				}
			}
			a_disc.samples = n;

			// Every hit along a sample's ray, from the wanted camera position
			// to the player: the sample is blocked when the occluder is
			// among them, whatever else is on the ray -- a wall in front of a
			// door does not hide the door from the count -- and the hit is
			// within partDistance of the sweep's own hit: the same part of
			// the occluder. A house is one reference; its pillar and its wall
			// are told apart by where they were hit. The share is what the
			// occluder blocks over all the samples.
			int              taken = 0, clear = 0;
			const float      partDistance = g_settings.partDistance;
			std::vector<Hit> hits;
			(void)a_world;
			for (int i = 0; i < n; ++i) {
				const RE::NiPoint3& sample = a_disc.point[i];
				const RE::NiPoint3  start = eye;
				const RE::NiPoint3  to = sample;

				AllHits(start, to, a_scale, hits);
				bool                      there = false;
				const bool                unknown = false;
				RE::hkpWorldRayCastOutput drawn;  // the occluder's hit, or the nearest, for drawing
				for (const auto& hit : hits) {
					auto*      hitRef = RE::TESHavokUtilities::FindCollidableRef(*hit.root);
					const auto hitId = hitRef ? hitRef->GetFormID() : 0u;
					if (hitId == id) {
						const RE::NiPoint3 at = start + (to - start) * hit.fraction;
						if (Length(at - a_at) <= partDistance) {
							there = true;
							drawn.hitFraction = hit.fraction;
							drawn.rootCollidable = hit.root;
							break;
						}
					}
					if (!drawn.rootCollidable) {
						drawn.hitFraction = hit.fraction;
						drawn.rootCollidable = hit.root;
					}
				}
				a_disc.hit[i] = there;
				Record(RayKind::Disc, start, to, drawn, there, a_forWhisker);
				if (there) {
					++taken;
				} else if (!unknown) {
					++clear;
				}
			}
			const int known = taken + clear;
			return known > 0 ? (taken * 100) / known : 100;
		}

		// --- the engine's collector, fronted ------------------------------------------
		//
		// CommonLib declares hkpCdPointCollector's destructor and Reset but
		// defines neither, so the ABI is laid out here: vtable, then the
		// early-out distance at 8.
		class ProxyCollector final
		{
		public:
			explicit ProxyCollector(RE::hkpCdPointCollector& a_inner) :
				inner(a_inner)
			{
				earlyOutDistance = a_inner.earlyOutDistance;
			}
			virtual ~ProxyCollector() = default;

			virtual void AddCdPoint(const RE::hkpCdPoint& a_point)
			{
				const auto* root = RootOf(a_point.cdBodyB);
				const auto  at = ToGame(a_point.contact.position, scale);
				auto*       ref = root ? RE::TESHavokUtilities::FindCollidableRef(*root) : nullptr;
				const auto  normal = Normalized({ X(a_point.contact.separatingNormal), Y(a_point.contact.separatingNormal),
					 Z(a_point.contact.separatingNormal) });

				(void)normal;
				const Verdict verdict = Judge(world, root, ref, at, g_castTo, scale, false, g_settings);
				const bool    ownBody = root && root->GetCollisionLayer() == RE::COL_LAYER::kCharController;
				const bool    drop = !verdict.stops && !ownBody;

				if (g_verbose) {
					spdlog::info("  hit {:.3f} cover {}% {}{}{} {}", W(a_point.contact.separatingNormal), verdict.cover,
						verdict.whole ? "through by layer " : "", verdict.isSmall ? "small " : "", drop ? "DROPPED" : "kept", Describe(root));
				}
				if (drop) {
					dropped.push_back({ ref, at, verdict.whole });
					return;
				}
				inner.AddCdPoint(a_point);
				earlyOutDistance = inner.earlyOutDistance;
			}

			virtual void Reset()
			{
				inner.Reset();
				earlyOutDistance = inner.earlyOutDistance;
			}

			struct Dropped
			{
				RE::TESObjectREFR* ref;
				RE::NiPoint3       at;
				bool               whole;
			};

			float                    earlyOutDistance;  // 08
			RE::hkpCdPointCollector& inner;
			float                    scale = 1.0f;
			const RE::hkpWorld*      world = nullptr;
			std::vector<Dropped>     dropped;
		};
		static_assert(offsetof(ProxyCollector, earlyOutDistance) == offsetof(RE::hkpCdPointCollector, earlyOutDistance));

		std::vector<ProxyCollector::Dropped> g_dropped;  // this update's

		// --- the hooks ----------------------------------------------------------------
		void UpdateCameraCollisionHook(RE::ThirdPersonState* a_this)
		{
			g_settings = settings::Current();
			if (!g_settings.enabled) {
				g_originalUpdate(a_this);
				return;
			}
			const auto n = g_updates.fetch_add(1, std::memory_order_relaxed);
			g_verbose = g_settings.logVerbose && (n < 10 || (n % 60) == 0);

			g_dropped.clear();
			g_building = Frame{};
			g_building.update = n;

			g_inCameraCollision = true;
			g_originalUpdate(a_this);
			g_inCameraCollision = false;

			for (const auto& dropped : g_dropped) {
				fade::Around(dropped.ref, dropped.at, dropped.whole, g_settings);
			}
			fade::Update(g_settings);
			ForgetOldDecisions();
			motion::Apply(a_this, g_castThisUpdate, g_settings);

			if (Recording()) {
				g_building.rays.push_back({ g_castFrom, g_castTo, a_this->translation, true, true, false, RayKind::Cast });
				g_building.bounds = fade::Bounds();
			}
			{
				std::lock_guard<std::mutex> lock(g_frameMutex);
				g_frame = std::move(g_building);
				g_building = Frame{};
			}
			g_castThisUpdate = false;

			if (g_verbose) {
				spdlog::info("update {} -> camera at ({:.1f}, {:.1f}, {:.1f}), held at {:.2f} of the cast", n,
					a_this->translation.x, a_this->translation.y, a_this->translation.z, motion::Pull());
			}
		}

		void LinearCastHook(const RE::hkpWorld* a_world, const RE::hkpCollidable* a_colA, const RE::hkpLinearCastInput& a_input,
			RE::hkpCdPointCollector& a_castCollector, RE::hkpCdPointCollector* a_startCollector)
		{
			if (!g_inCameraCollision) {
				g_originalLinearCast(a_world, a_colA, a_input, a_castCollector, a_startCollector);
				return;
			}

			const float scale = RE::bhkWorld::GetWorldScaleInverse();
			if (a_colA) {
				g_castFilter = a_colA->broadPhaseHandle.collisionFilterInfo;
				if (const auto* motion = static_cast<const RE::hkMotionState*>(a_colA->motion)) {
					g_castFrom = ToGame(motion->transform.translation, scale);
					g_castTo = ToGame(a_input.to, scale);
					g_castLength = Length(g_castTo - g_castFrom);
					g_castThisUpdate = true;
				}
			}

			ProxyCollector proxy(a_castCollector);
			proxy.scale = scale;
			proxy.world = a_world;
			g_originalLinearCast(a_world, a_colA, a_input, reinterpret_cast<RE::hkpCdPointCollector&>(proxy), a_startCollector);
			a_castCollector.earlyOutDistance = proxy.earlyOutDistance;
			g_dropped = std::move(proxy.dropped);

			if (g_castThisUpdate) {
				whiskers::Cast(a_world, scale, g_settings);
			}
		}

		template <class Fn>
		bool Hook(std::uint64_t a_id, Fn* a_hook, Fn*& a_original, const char* a_name)
		{
			const REL::Relocation<std::uintptr_t> target{ REL::ID(a_id) };
			void*                                 trampoline = nullptr;
			if (const auto status = MH_CreateHook(reinterpret_cast<void*>(target.address()),
					reinterpret_cast<void*>(a_hook), &trampoline);
				status != MH_OK) {
				spdlog::error("collision: MH_CreateHook failed for {}: {}", a_name, MH_StatusToString(status));
				return false;
			}
			a_original = reinterpret_cast<Fn*>(trampoline);
			if (const auto status = MH_EnableHook(reinterpret_cast<void*>(target.address())); status != MH_OK) {
				spdlog::error("collision: MH_EnableHook failed for {}: {}", a_name, MH_StatusToString(status));
				return false;
			}
			spdlog::info("collision: hooked {} at rva 0x{:X}", a_name, target.offset());
			return true;
		}
	}

	Verdict Judge(const RE::hkpWorld* a_world, const RE::hkpCollidable* a_root, RE::TESObjectREFR* a_ref,
		const RE::NiPoint3& a_at, const RE::NiPoint3& a_eye, float a_scale, bool a_forWhisker,
		const settings::Values& a_settings)
	{
		Verdict verdict;
		if (!a_root) {
			return verdict;
		}
		const auto layer = a_root->GetCollisionLayer();
		if (layer == RE::COL_LAYER::kCharController) {
			verdict.stops = false;
			return verdict;
		}
		const bool ground = layer == RE::COL_LAYER::kTerrain || layer == RE::COL_LAYER::kGround ||
		                    (a_root->shape && a_root->shape->type == RE::hkpShapeType::kHeightField);
		const auto rule = ground ? settings::Rule::Stop : RuleFor(layer);
		if (rule == settings::Rule::Through) {
			verdict.stops = false;
			verdict.whole = true;
			return verdict;
		}
		if (rule == settings::Rule::Stop) {
			return verdict;
		}

		DiscView disc;
		verdict.cover = DiscCover(a_world, a_at, a_eye, a_ref, a_scale, a_forWhisker, disc);
		const bool tooSmallOnTheDisc = Decide(a_ref, verdict.cover);
		verdict.isSmall = fade::Fadeable(a_ref, a_at, a_settings);
		verdict.stops = !tooSmallOnTheDisc && !verdict.isSmall;
		disc.dropped = !verdict.stops;
		if (Recording()) {
			g_building.discs.push_back(disc);
		}
		return verdict;
	}

	const RE::NiPoint3& CastFrom() { return g_castFrom; }
	const RE::NiPoint3& CastTo() { return g_castTo; }
	float               CastLength() { return g_castLength; }

	void CastRay(const RE::hkpWorld* a_world, const RE::NiPoint3& a_from, const RE::NiPoint3& a_to, float a_scale,
		RE::hkpWorldRayCastOutput& a_output)
	{
		RE::hkpWorldRayCastInput input;
		input.from = ToHavok(a_from, 1.0f / a_scale);
		input.to = ToHavok(a_to, 1.0f / a_scale);
		input.filterInfo = g_castFilter;
		input.enableShapeCollectionFilter = false;
		EngineCastRay()(a_world, input, a_output);
	}

	Frame LastFrame()
	{
		std::lock_guard<std::mutex> lock(g_frameMutex);
		return g_frame;
	}

	bool Recording()
	{
		return g_settings.drawDiscs || g_settings.drawWhiskers || g_settings.drawRays || g_settings.drawBounds;
	}

	void Record(RayKind a_kind, const RE::NiPoint3& a_from, const RE::NiPoint3& a_to, const RE::hkpWorldRayCastOutput& a_output,
		bool a_counts, bool a_forWhisker)
	{
		if (!Recording()) {
			return;
		}
		RayView view{ a_from, a_to, a_to, a_output.HasHit(), a_counts, a_forWhisker, a_kind };
		if (view.hit) {
			view.hitAt = a_from + (a_to - a_from) * a_output.hitFraction;
		}
		g_building.rays.push_back(view);
	}

	bool Install()
	{
		if (const auto status = MH_Initialize(); status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) {
			spdlog::error("collision: MH_Initialize failed: {}", MH_StatusToString(status));
			return false;
		}
		return Hook(kUpdateCameraCollisionID, &UpdateCameraCollisionHook, g_originalUpdate, "ThirdPersonState::UpdateCameraCollision") &&
		       Hook(kLinearCastID, &LinearCastHook, g_originalLinearCast, "hkpWorld::LinearCast");
	}
}
