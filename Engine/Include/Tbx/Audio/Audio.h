#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Ids/Uid.h"
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

    /// <summary>
    /// Represents raw audio data that can be attached to toys and scheduled for playback.
    /// </summary>
    class TBX_EXPORT Audio
    {
    public:
        using SampleData = std::vector<std::uint8_t>;

    public:
        Audio() = default;
        Audio(SampleData data, AudioFormat format);

        /// <summary>
        /// Requests that the audio begin playing.
        /// </summary>
        void Play();

        /// <summary>
        /// Stops the audio from playing.
        /// </summary>
        void Stop();

        /// <summary>
        /// Determines whether playback has been requested for this audio instance.
        /// </summary>
        bool IsPlaying() const;

    public:
        Uid Id = Uid::Generate();
        SampleData Data = {};
        AudioFormat Format = {};
        float Pitch = 1.0f;
        float PlaybackSpeed = 1.0f;
        bool Loop = false;

    private:
        bool _isPlaying = false;
    };
}
