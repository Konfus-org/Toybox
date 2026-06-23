#pragma once
#include "engine_services.h"
#include "view_manager.h"
#include "tbx/systems/files/json.h"
#include "tbx/utils/result.h"

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

      private:
        EngineServices& _services;
        ViewManager& _views;
    };
}
