#pragma once
#include "engine_services.h"
#include "view_manager.h"
#include "tbx/systems/files/json.h"
#include "tbx/utils/result.h"
#include <cstdint>
#include <unordered_set>

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: Resolves viewport picking against the active world for a given view's camera — a
    /// single-click ray pick and a marquee box-select.
    /// @details
    /// Ownership: Stateless; borrows the engine services and view manager. Thread Safety: Main-thread
    /// only (request handling).
    class PickService
    {
      public:
        PickService(EngineServices& services, ViewManager& views);

        // Picks the entity under a normalized click { view, u, v } (top-left origin): builds a ray
        // from the view's camera and returns the nearest triangle-precise static-mesh hit as { id }
        // (or { id: null } for empty space).
        Result pick(const tbx::Json& params, tbx::Json& out_reply);

        // Box-selects the static meshes whose world-bounds centre projects inside the normalized
        // marquee { view, u0,v0,u1,v1 } (top-left origin); replies { ids: [...] }.
        Result pick_rect(const tbx::Json& params, tbx::Json& out_reply);

        // For each entity in { view, ids }, whether it is hidden behind other renderer geometry from the
        // view's camera: casts a ray from the camera to each entity and reports whether any other mesh
        // blocks it. Replies { occluded: [bool] } aligned with the input ids. Batched (one pass over the
        // scene's meshes) so the billboard overlay can refresh all its icons in a single call. CPU mesh
        // test, so it works in the paused editor (unlike a physics raycast, whose bodies only exist while
        // playing).
        Result query_occlusion(const tbx::Json& params, tbx::Json& out_reply);

      private:
        EngineServices& _services;
        ViewManager& _views;

        // Renderer model handles that failed to load (dangling/unregistered, e.g. a runtime-only handle):
        // remembered so per-frame picking/occlusion don't reload — and re-log — them every call.
        std::unordered_set<std::uint64_t> _unloadable_models = {};
    };
}
