#pragma once
#include "tbx/types/components/component.h"
#include "tbx/types/components/lods.generated.h"

namespace tbx
{
    /// @brief
    /// Purpose: Defines a model handle to use within a specific distance band.
    /// @details
    /// Ownership: Stores handles by value; does not own loaded model assets.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    [[serializable]];
    struct TBX_API Lod
    {
        [[asset("fbx", "obj", "gltf", "glb")]]
        Handle handle = {};

        float max_distance = 0.0f;
    };

    /// @brief
    /// Purpose: Stores mesh LOD selection data for a renderable entity.
    /// @details
    /// Ownership: Owns the LOD collection by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    [[serializable]];
    [[icon("Layers", Color::GREY)]];
    struct TBX_API Lods : Component
    {
        std::vector<Lod> values = {};

        float render_distance = 0.0f;
    };
}
