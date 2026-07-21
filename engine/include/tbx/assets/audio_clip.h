#pragma once
#include "tbx/core/result.h"
#include "tbx/core/typedefs.h"
#include <cstddef>
#include <span>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Decoded audio asset: interleaved float samples.
    struct AudioClip
    {
        int channels = 0;
        int sample_rate = 0;
        std::vector<float> samples = {};
    };

    /// @brief
    /// Purpose: Decodes a RIFF/WAV payload (PCM16 or float32, mono/stereo) into an AudioClip —
    /// pure over bytes so it unit-tests without files.
    Result<AudioClip> parse_wav(std::span<const std::byte> bytes);
}
