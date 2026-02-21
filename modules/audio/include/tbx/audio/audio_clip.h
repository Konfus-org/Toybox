#pragma once
#include "tbx/common/int.h"
#include "tbx/tbx_api.h"
#include <vector>

namespace tbx
{
    /// <summary>
    /// Purpose: Represents raw audio sample data for asset loading.
    /// </summary>
    /// <remarks>
    /// Ownership: Instances own their sample buffers.
    /// Thread Safety: Safe to move across threads; synchronize shared mutation externally.
    /// </remarks>
    struct TBX_API AudioClip
    {
        uint32 sample_rate = 44100;
        uint16 channels = 2;
        std::vector<float> samples = {};
    };
}
