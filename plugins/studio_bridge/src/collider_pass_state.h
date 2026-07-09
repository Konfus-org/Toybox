#pragma once
#include "tbx/systems/graphics/gizmos.h"
#include "tbx/types/typedefs.h"
#include "tbx/types/uuid.h"
#include "tbx/types/vectors.h"
#include <memory>
#include <unordered_map>
#include <vector>

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: One entity's cached convex-collider wireframe: the cooked hull triangles queried
    /// from the physics backend (shape-local, scale baked), plus the model/scale they were built
    /// from so a changed mesh or scale re-queries. Querying joins the in-flight physics step, so the
    /// cache keeps the steady state free of per-frame joins.
    /// @details
    /// Ownership: Value entry inside ColliderPassState's cache. Thread Safety: Main-thread only.
    struct ConvexHullCacheEntry
    {
        tbx::Uuid model = {};
        tbx::Vec3 scale = tbx::Vec3(1.0F, 1.0F, 1.0F);
        std::vector<tbx::Vec3> triangle_vertices = {};

        // The submission pass that last drew this entry; entries untouched by the current pass are
        // pruned (deselected or deleted entities don't pin their hulls).
        uint64 last_used_pass = 0U;
    };

    /// @brief
    /// Purpose: The collider-wireframe overlay's state: its own immediate-mode Gizmos batch, its
    /// registered render-pass id, and the convex-hull wireframe cache. Plain state:
    /// collider_pass_ops owns the behavior (pass lifecycle + the per-frame wireframe submission).
    /// @details
    /// Ownership: Owned by the plugin by value; `gizmos` is shared with the registered pass's execute
    /// callback so an in-flight frame keeps the batch alive. Thread Safety: The main thread fills the
    /// batch each frame; the pass renders it on the render lane (the Gizmos batch is mutex-guarded
    /// across the two).
    struct ColliderPassState
    {
        // The bridge's own gizmo batch, separate from the shared service so the collider wireframes
        // are an independent pass rather than sharing the transform-gizmo overlay's batch. Null until
        // register_collider_pass builds it.
        std::shared_ptr<tbx::Gizmos> gizmos = {};

        // Id of the collider overlay pass on the Rendering service, kept so it can be removed before
        // the plugin unloads. Invalid until registered.
        tbx::Uuid pass = {};

        // Cached convex mesh-collider hulls by entity, pruned against submission_pass each frame.
        std::unordered_map<tbx::Uuid, ConvexHullCacheEntry> convex_hulls = {};

        // Monotonic per-submission counter stamping the cache entries still in use.
        uint64 submission_pass = 0U;
    };
}
