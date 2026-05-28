#pragma once
#include "tbx/tbx_api.h"
#include "tbx/types/components/component.h"
#include "tbx/types/components/material_instance.h"
#include "tbx/types/typedefs.h"

namespace tbx
{
    /// @brief
    /// Purpose: Selects the runtime mesh used to render the sky.
    /// @details
    /// Ownership: Value type.
    /// Thread Safety: Safe to copy between threads.
    enum class SkyType : uint8
    {
        BOX = 0,
        SPHERE = 1
    };

    /// @brief
    /// Purpose: Stores the sky material instance used for environment rendering.
    /// @details
    /// Ownership: Owns the material instance by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API Sky : Component
    {
        Sky() = default;
        Sky(MaterialInstance material, SkyType type = SkyType::SPHERE);

        MaterialInstance material = {};
        SkyType type = SkyType::SPHERE;
    };

    TBX_REGISTER_SERIALIZABLE_STRUCT(Sky, id, material, type)
}
