#pragma once
#include "tbx/api.h"
#include "tbx/ecs/block.h"
#include "tbx/math/math.h"

namespace tbx
{
    class Sandbox;

    /// @brief
    /// Purpose: A toy wearing this block turns to face the camera each frame. Billboarding is
    /// opt-in — nothing rotates toward the camera without a Billboard block.
    struct TBX_API Billboard : Block
    {
        // Upright: only yaw toward the camera (labels, sprites). Off = face it fully.
        bool lock_y = true;

        Billboard& set_lock_y(bool value)
        {
            lock_y = value;
            return *this;
        }
    };

    /// @brief
    /// Purpose: Turns every Billboard toy to face the given camera position (the caller passes
    /// the active camera's world position; tbx::run() does this each frame before rendering).
    TBX_API void update_billboards(Sandbox& sandbox, const Vec3& camera_position);
}
