#pragma once
#include "tbx/tbx_api.h"
#include "tbx/types/assets/asset.h"
#include "tbx/types/typedefs.h"
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Represents raw audio sample data for asset loading.
    /// @details
    /// Ownership: Instances own their sample buffers.
    /// Thread Safety: Safe to move across threads; synchronize shared mutation externally.
    [[tbx::serializable]];
    [[tbx::version(1U)]];
    struct TBX_API AudioClip : Asset
    {
        uint32 sample_rate = 44100;
        uint16 channels = 2;
        std::vector<float> samples = {};
    };

}

#include "tbx/types/assets/audio_clip.generated.h"
