#pragma once
#include "tbx/api.h"
#include "tbx/assets/asset.h"
#include "tbx/reflection/attributes.h"
#include "tbx/utils/result.h"
#include "tbx/utils/typedefs.h"
#include <cstddef>
#include <filesystem>
#include <span>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Decoded audio asset: interleaved float samples.
    struct TBX_SERIALIZABLE(SerializerFormat::CUSTOM, reader=&deserialize_clip) TBX_DLL_EXPORT AudioClip : Asset
    {
        int channels = 0;
        int sample_rate = 0;
        std::vector<float> samples = {};
    };

    /// @brief
    /// Purpose: Decodes a RIFF/WAV payload (PCM16 or float32, mono/stereo) into a AudioClip —
    /// pure over bytes so it unit-tests without files.
    TBX_DLL_EXPORT Result<AudioClip> parse_wav(std::span<const std::byte> bytes);

    /// @brief
    /// Purpose: AudioClip's registered reader (WAV) — call it through
    /// deserialize<AudioClip>(path).
    TBX_DLL_EXPORT Result<AudioClip> deserialize_clip(const std::filesystem::path& path);
}
