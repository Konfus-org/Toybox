#pragma once
#include "tbx/common/typedefs.h"
#include "tbx/tbx_api.h"
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Represents raw audio sample data for asset loading.
    /// @details
    /// Ownership: Instances own their sample buffers.
    /// Thread Safety: Safe to move across threads; synchronize shared mutation externally.
    struct TBX_API AudioClip
    {
        uint32 sample_rate = 44100;
        uint16 channels = 2;
        std::vector<float> samples = {};
    };
}
