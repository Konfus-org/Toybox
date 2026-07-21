#pragma once
#include "tbx/systems/graphics/camera_view.h"
#include "tbx/tbx_api.h"
#include "tbx/types/assets/world.h"
#include "tbx/types/uuid.h"
#include <unordered_set>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: One entity projected into a view's screen space: its id, normalized image coordinates
    /// (top-left origin, both 0..1, only produced when the entity is in front of the camera) and the
    /// world-space distance from the camera (e.g. for distance fade).
    struct EntityScreenPosition
    {
        Uuid id = {};
        float u = 0.0F;
        float v = 0.0F;
        float depth = 0.0F;
    };

    /// @brief
    /// Purpose: Projects every Transform-bearing entity in `world` to `camera`'s normalized screen
    /// coordinates (top-left origin), skipping any entity behind the camera. When `only` is non-null,
    /// projects just those entity ids (how a caller anchoring a handful of overlays skips sweeping a
    /// large world). A thin convenience over the per-point pieces it builds on —
    /// CameraView::project_to_screen (the projection) and tbx::distance (the depth) — that computes
    /// the view-projection once so a batch projects cheaply. Useful for viewport overlays (the
    /// editor's entity billboards), debug labels, world-anchored UI, and the like.
    /// @details
    /// Ownership: Returns a value vector the caller owns. Thread Safety: Reads the world; don't mutate
    /// that world concurrently.
    TBX_API std::vector<EntityScreenPosition> project_entities_to_screen(
        const CameraView& camera,
        World& world,
        const std::unordered_set<Uuid>* only = nullptr);
}
