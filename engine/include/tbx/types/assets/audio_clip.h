#pragma once
#include "tbx/types/assets/asset.h"

namespace tbx
{
    /// @brief
    /// Purpose: Represents raw audio sample data for asset loading.
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
