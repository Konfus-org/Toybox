#pragma once
#include "tbx/api.h"
#include "tbx/ecs/block.h"
#include "tbx/math/math.h"
#include "tbx/reflection/attributes.h"

namespace tbx
{
    /// @brief
    /// Purpose: A toy wearing this block turns to face the camera each frame. Billboarding is
    /// opt-in — nothing rotates toward the camera without a Billboard block. The facing update
    /// runs inside update_ecs() (see ecs.cpp) each frame before rendering.
    struct TBX_SERIALIZABLE() TBX_EXPOSED_TO_SCRIPTING TBX_DLL_EXPORT Billboard : Block
    {
        // Upright: only yaw toward the camera (labels, sprites). Off = face it fully.
        bool lock_y = true;

        Billboard& set_lock_y(bool value)
        {
            lock_y = value;
            return *this;
        }
    };
}
