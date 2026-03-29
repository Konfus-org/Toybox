#pragma once
#include "tbx/common/int.h"
#include "tbx/tbx_api.h"
#include <cstdint>

namespace tbx
{
    /// @brief
    /// Purpose: Stores width/height pairs for windowing and layout sizing.
    /// @details
    /// Ownership: Value type; callers own their copies of sizes.
    /// Thread Safety: Not inherently thread-safe; synchronize shared mutable access.
    struct TBX_API Size
    {
        /// @brief
        /// Purpose: Computes an aspect ratio for projection math.
        /// @details
        /// Ownership: Returns a value; no ownership transfer.
        /// Thread Safety: Thread-safe for immutable access.
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
