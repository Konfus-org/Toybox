#pragma once
#include "engine_services.h"
#include "view_manager.h"
#include "tbx/systems/files/json.h"
#include "tbx/utils/result.h"
#include <cstdint>
#include <functional>
#include <unordered_set>

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: Resolves the editor's viewport queries against a given view's camera and world — a
    /// single-click ray pick, a marquee box-select, the billboard-overlay entity-to-screen projection,
    /// and the billboard-overlay occlusion test.
    /// @details
    /// Ownership: Holds non-owning references to the engine services and the view manager; owns
    /// only its set of known-unloadable model handles. Thread Safety: Main-thread only (request
    /// handling).
    class EntitySelectionHandler
    {
      public:
        EntitySelectionHandler(EngineServices& services, ViewManager& views);

      public:
        /// @brief Picks the entity under a normalized click { view, u, v } (top-left origin): builds a
        /// ray from the view's camera and returns the nearest triangle-precise static-mesh hit as
        /// { id } (or { id: null } for empty space).
        Result pick(const tbx::Json& params, tbx::Json& out_reply);

        /// @brief Box-selects the static meshes whose world-bounds centre projects inside the
        /// normalized marquee { view, u0,v0,u1,v1 } (top-left origin); replies { ids: [...] }.
        Result pick_rect(const tbx::Json& params, tbx::Json& out_reply);

        /// @brief Projects every entity in { view } to the view's normalized screen space (top-left
        /// origin), replying { items: [{ id, u, v, depth }] } for those in front of the camera. The
        /// editor polls this to position its billboard overlay (it owns the cadence); the engine only
        /// answers when asked.
        Result project_entities(const tbx::Json& params, tbx::Json& out_reply);

        /// @brief For each entity in { view, ids }, whether it is hidden behind other renderer geometry
        /// from the view's camera. Replies { occluded: [bool] } aligned with the input ids. Batched
        /// (one pass over the scene's meshes) so the billboard overlay can refresh all its icons in a
        /// single call. CPU mesh test, so it works in the paused editor (unlike a physics raycast,
        /// whose bodies only exist while playing).
        Result query_occlusion(const tbx::Json& params, tbx::Json& out_reply);

      private:
        std::reference_wrapper<EngineServices> _services;
        std::reference_wrapper<ViewManager> _views;

        // Renderer model handles that failed to load (dangling/unregistered, e.g. a runtime-only
        // handle): remembered so per-frame picking/occlusion don't reload — and re-log — them every
        // call.
        std::unordered_set<std::uint64_t> _unloadable_models = {};
    };
}
