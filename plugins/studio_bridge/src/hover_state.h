#pragma once
#include "tbx/types/uuid.h"
#include "tbx/types/vectors.h"
#include <memory>
#include <string>

namespace tbx
{
    class World;
}

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: The bridge's hover tracking — which entity is under the cursor of the focused editor
    /// view, plus the cursor/camera snapshot of the last raycast so a static frame (nothing moved)
    /// skips the raycast entirely. Plain state: hover_ops owns the behavior (the per-update raycast,
    /// the editor.hovered tag swap, and the view.hover notification).
    /// @details
    /// Ownership: Owned by the plugin by value; holds a weak reference to the world the tag was
    /// stamped in (so a swapped/closed world never pins). Thread Safety: Main thread only.
    struct HoverState
    {
        // The currently hovered entity (invalid = none), the world it was tagged in, and the view
        // the hover belongs to (echoed in the view.hover notification).
        tbx::Uuid hovered = {};
        std::weak_ptr<tbx::World> hovered_world = {};
        std::string view = {};

        // The cursor + camera pose of the last raycast; when none of them changed, the scene under
        // the cursor is assumed unchanged and the raycast is skipped (edits mid-hover refresh on the
        // next cursor move, which is when the user cares again).
        float last_u = 0.0F;
        float last_v = 0.0F;
        tbx::Vec3 last_camera_position = {};
        tbx::Quat last_camera_rotation = tbx::Quat(1.0F, 0.0F, 0.0F, 0.0F);
        bool has_last = false;
    };
}
