#pragma once
#include "view_input.h"
#include "view_stream.h"
#include "tbx/types/render_texture.h"
#include "tbx/types/typedefs.h"
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: The editor's render views — the view-stream collection, each view's camera (mirrored
    /// onto a transient camera entity injected into the view's world, which the engine renders like
    /// any world camera), and the cross-process shared GPU surface lifecycle. Nothing here renders:
    /// the engine renders the world cameras and the gizmo/collider/selection passes draw the
    /// overlays. Plain state: view_ops owns the behavior (view start/stop, camera seeding/sync, the
    /// present-callback surface lifecycle, and the view-resolution queries every other domain calls).
    /// Other subsystems reach the views through with_views_locked / resolve_view_camera.
    /// @details
    /// Ownership: Owned by the plugin by value. Owns the streams (and through them each view's render
    /// texture + external-camera registration), their forwarded input, and the pending shared-surface
    /// destroy queue. Thread Safety: The view collection is guarded by `mutex`; the surface lifecycle
    /// runs on the render lane (ensure_view_surfaces, inside the present callback) under that lock,
    /// the rest on the main thread. Non-copyable/non-movable (the mutex pins it), so its address stays
    /// stable for the present callback that captures it.
    struct ViewState
    {
        uint32 next_view_index = 0U;
        // Hands each new asset-preview world a unique non-zero id (0 is reserved for the active world).
        uint32 next_world_id = 1U;

        // Every non-active world the editor can address by id: preview worlds (owned by their view) and
        // worlds loaded via world.load (owned by the WorldManager). Keyed by the stable world id handed
        // out from next_world_id; id 0 (the active world) is never a key. resolve_world_by_id reads this;
        // per-entity ops fall back to scanning it (find_preview_world_with) when no world id is given.
        std::unordered_map<uint32, std::shared_ptr<tbx::World>> worlds = {};

        std::vector<std::unique_ptr<ViewStream>> streams = {};
        // The forwarded input for each live view, keyed by view name — created and dropped with the view
        // so it never outlives its stream. Kept off the stream so a ViewStream is purely render state.
        std::unordered_map<std::string, ViewInput> inputs = {};
        // Shared-texture resource ids of stopped views awaiting teardown on the render lane.
        std::vector<tbx::GpuId> pending_shared_destroys = {};
        mutable std::mutex mutex = {};
    };
}
