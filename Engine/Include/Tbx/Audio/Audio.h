#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Ids/Uid.h"
#include "Tbx/Memory/Refs.h"
#include <cstdint>
#include <vector>

namespace Tbx
{
    /// <summary>
    /// Identifies the format of individual audio samples stored in an Audio asset.
    /// </summary>
    enum class AudioSampleFormat
    {
        Unknown,
        UInt8,
        Int16,
        Int32,
        Float32
    };

    /// <summary>
    /// Describes the sample layout for an Audio asset.
    /// </summary>
    struct AudioFormat
    {
        AudioSampleFormat SampleFormat = AudioSampleFormat::Unknown;
        int SampleRate = 0;
        int Channels = 0;
    };

    using SampleData = std::vector<std::uint8_t>;

    /// <summary>
    /// Represents raw audio data that can be attached to toys and scheduled for playback.
    /// </summary>
    class TBX_EXPORT Audio
    {
    public:
        Audio() = default;
        Audio(SampleData data, AudioFormat format);

    public:
        SampleData Data = {};
        AudioFormat Format = {};
        Uid Id = Uid::Generate();
    };

    class TBX_EXPORT AudioSource
    {
    public:
        AudioSource() = default;
        AudioSource(Ref<Audio> clip) : Clip(clip) {}

    public:
        Ref<Audio> Clip;
        bool Playing = false;
        bool Looping = false;
        float Volume = 1.0f;
        float Pitch = 1.0f;
        float PlaybackSpeed = 1.0f;
    };
}
