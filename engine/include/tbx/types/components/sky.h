#pragma once
#include "tbx/types/components/component.h"
#include "tbx/types/components/material_instance.h"

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
    [[tbx::serializable]];
    struct TBX_API Sky : Component
    {
        Sky() = default;
        Sky(MaterialInstance material, SkyType type = SkyType::SPHERE);

        [[tbx::prop]]
        MaterialInstance material = {};

        [[tbx::prop]]
        SkyType type = SkyType::SPHERE;
    };
}

#include "tbx/types/components/sky.generated.h"
