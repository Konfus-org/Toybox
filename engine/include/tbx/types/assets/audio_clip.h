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
        uint32 sample_rate = 44100;
        uint16 channels = 2;
        std::vector<float> samples = {};
    };

    /// @brief
    /// Purpose: Provides audio-specific read parameters for serialized audio assets.
    /// @details
    /// Ownership: Value type settings owned by the caller.
    /// Thread Safety: Safe to copy between threads.
    struct AudioLoadParameters
    {
        bool operator==(const AudioLoadParameters& other) const = default;
    };

    AudioLoadParameters load_parameters_of(const AudioClip&);
}
