#pragma once
#include "tbx/common/handle.h"
#include "tbx/graphics/material.h"
#include "tbx/tbx_api.h"
#include <vector>

namespace tbx
{
    /// <summary>
    /// Purpose: Defines a model handle to use within a specific distance band.
    /// </summary>
    /// <remarks>
    /// Ownership: Stores handles by value; does not own loaded model assets.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    /// </remarks>
    struct TBX_API RendererLod
    {
        Handle handle = {};
        float max_distance = 0.0f;
    };

    /// <summary>
    /// Purpose: Stores mesh LOD selection data for a renderable entity.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns the LOD collection by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    /// </remarks>
    struct TBX_API Lods
    {
        std::vector<RendererLod> values = {};
        float render_distance = 0.0f;
    };
}
