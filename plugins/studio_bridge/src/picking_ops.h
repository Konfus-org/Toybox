#pragma once
#include "tbx/systems/files/json.h"
#include "tbx/utils/result.h"

namespace tbx::studio_bridge
{
    struct EngineServices;
    struct PickingState;
    struct ViewState;

    /// @brief Picks the entity under a normalized click { view, u, v } (top-left origin): builds a
    /// ray from the view's camera and returns the nearest triangle-precise static-mesh hit as
    /// { id } (or { id: null } for empty space).
    Result pick(
        PickingState& picking,
        const EngineServices& services,
        ViewState& views,
        const tbx::Json& params,
        tbx::Json& out_reply);

    /// @brief Box-selects the entities whose world-bounds centre projects inside the normalized
    /// marquee { view, u0,v0,u1,v1 } (top-left origin); replies { ids: [...] }.
    Result pick_rect(
        const EngineServices& services,
        ViewState& views,
        const tbx::Json& params,
        tbx::Json& out_reply);

    /// @brief Projects every entity in { view } to the view's normalized screen space (top-left
    /// origin), replying { items: [{ id, u, v, depth }] } for those in front of the camera. The
    /// editor polls this to position its billboard overlay (it owns the cadence); the engine only
    /// answers when asked.
    Result project_entities(
        const EngineServices& services,
        ViewState& views,
        const tbx::Json& params,
        tbx::Json& out_reply);

    /// @brief For each entity in { view, ids }, whether it is hidden behind other renderer geometry
    /// from the view's camera. Replies { occluded: [bool] } aligned with the input ids. Batched
    /// (one pass over the scene's meshes) so the billboard overlay can refresh all its icons in a
    /// single call. CPU mesh test, so it works in the paused editor (unlike a physics raycast,
    /// whose bodies only exist while playing).
    Result query_occlusion(
        PickingState& picking,
        const EngineServices& services,
        ViewState& views,
        const tbx::Json& params,
        tbx::Json& out_reply);
}
