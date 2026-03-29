#pragma once
#include "tbx/common/handle.h"
#include "tbx/graphics/material.h"
#include "tbx/tbx_api.h"
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Defines a model handle to use within a specific distance band.
    /// @details
    /// Ownership: Stores handles by value; does not own loaded model assets.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.

    struct TBX_API RendererLod
    {
        Handle handle = {};
        float max_distance = 0.0f;
    };

    /// @brief
    /// Purpose: Stores mesh LOD selection data for a renderable entity.
    /// @details
    /// Ownership: Owns the LOD collection by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.

    struct TBX_API Lods
    {
        std::vector<RendererLod> values = {};
        float render_distance = 0.0f;
    };
}
