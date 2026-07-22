#pragma once
#include "tbx/assets/asset.h"
#include "tbx/assets/load.h"
#include "tbx/utils/api.h"
#include "tbx/utils/result.h"
#include "tbx/utils/typedefs.h"
#include <cstddef>
#include <span>
#include <vector>

namespace tbx::audio
{
    /// @brief
    /// Purpose: Decoded audio asset: interleaved float samples.
    struct TBX_API AudioClip : assets::Asset
    {
        int channels = 0;
        int sample_rate = 0;
        std::vector<float> samples = {};
    };

    /// @brief
    /// Purpose: Decodes a RIFF/WAV payload (PCM16 or float32, mono/stereo) into an AudioClip —
    /// pure over bytes so it unit-tests without files.
    TBX_API Result<AudioClip> parse_wav(std::span<const std::byte> bytes);

}

namespace tbx::assets
{
    /// @brief
    /// Purpose: Loads a AudioClip from disk (implementation lives next to the type).
    template <>
    TBX_API Result<audio::AudioClip> load<audio::AudioClip>(const std::filesystem::path& path);
}
