#pragma once
#include "tbx/types/assets/asset.h"
#include "tbx/types/assets/audio_clip.generated.h"

namespace tbx
{
    /// @brief
    /// Purpose: Represents raw audio sample data for asset loading.
    [[serializable]];
    [[version(1U)]];
    struct TBX_API AudioClip : Asset
    {
        // Decoded by the audio loader from the source clip (a version-only asset), not persisted.
        [[do_not_serialize]]
        uint32 sample_rate = 44100;
        [[do_not_serialize]]
        uint16 channels = 2;
        [[do_not_serialize]]
        std::vector<float> samples = {};
    };
}
