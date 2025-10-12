#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Audio/Audio.h"

namespace Tbx
{
    /// <summary>
    /// Provides an abstraction for the platform specific audio playback implementation.
    /// </summary>
    class TBX_EXPORT IAudioMixer
    {
    public:
        virtual ~IAudioMixer() = default;

        virtual void Play(const Audio& audio) = 0;
        virtual void Stop(const Audio& audio) = 0;
        virtual void SetPitch(const Audio& audio, float pitch) = 0;
        virtual void SetPlaybackSpeed(const Audio& audio, float speed) = 0;
    };
}
