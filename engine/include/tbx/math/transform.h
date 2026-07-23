#pragma once
#include "tbx/api.h"
#include "tbx/ecs/block.h"
#include "tbx/math/math.h"
#include "tbx/reflection/attributes.h"

namespace tbx
{
    /// @brief
    /// Purpose: The one engine-core spatial block: local position/rotation/scale. Hierarchy
    /// lives on the Sandbox (set_parent/get_parent), not inside the block.
    struct TBX_SERIALIZABLE() TBX_DLL_EXPORT Transform : Block
    {
        Vec3 position = Vec3(0.0f, 0.0f, 0.0f);
        Quat rotation = Quat(1.0f, 0.0f, 0.0f, 0.0f);
        Vec3 scale = Vec3(1.0f, 1.0f, 1.0f);

        // Fluent setters — each returns *this so a Transform can be built in one chain, e.g.
        // Transform{}.set_position({0, 1, 0}).set_scale({2, 2, 2}). Designated-initializer
        // construction still works; these are just an alternative.
        Transform& set_position(Vec3 value) { position = value; return *this; }
        Transform& set_rotation(Quat value) { rotation = value; return *this; }
        Transform& set_scale(Vec3 value) { scale = value; return *this; }
    };
}
