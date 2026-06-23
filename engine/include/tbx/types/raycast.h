#pragma once
#include "tbx/types/ray.h"
#include "tbx/types/vectors.h"

namespace tbx
{
    /// @brief
    /// Purpose: Represents the result payload returned by a physics raycast query.
    /// @details
    /// Ownership: Value type containing non-owning entity identifiers.
    /// Thread Safety: Safe for concurrent reads; synchronize external mutation.
    struct TBX_API RaycastResult
    {
        bool has_hit = false;
        Uuid hit_entity_id = {};
        Vec3 hit_position = Vec3(0.0F, 0.0F, 0.0F);
        float hit_fraction = 1.0F;

        operator bool() const
        {
            return has_hit && hit_entity_id.is_valid();
        }
    };

    struct TBX_API RaycastQuery
    {
        Ray ray = {};
        float max_distance = 100.0F;
        bool ignore_entity = false;
        Uuid ignored_entity_id = {};
    };
}
