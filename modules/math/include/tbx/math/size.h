#pragma once
#include "tbx/common/int.h"
#include "tbx/tbx_api.h"
#include <cstdint>

namespace tbx
{
    /// <summary>Represents a 2D size in pixels.</summary>
    /// <remarks>Purpose: Stores width/height pairs for windowing and layout sizing.
    /// Ownership: Value type; callers own their copies of sizes.
    /// Thread Safety: Not inherently thread-safe; synchronize shared mutable access.</remarks>
    struct TBX_API Size
    {
        /// <summary>Gets the width-to-height ratio.</summary>
        /// <remarks>Purpose: Computes an aspect ratio for projection math.
        /// Ownership: Returns a value; no ownership transfer.
        /// Thread Safety: Thread-safe for immutable access.</remarks>
        float get_aspect_ratio() const
        {
            if (height == 0U)
            {
                return 0.0f;
            }

            return static_cast<float>(width) / static_cast<float>(height);
        }

        uint32 width = 0;
        uint32 height = 0;
    };
}
