#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Audio/Audio.h"
#include "Tbx/Math/Vectors.h"

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
        virtual void SetPosition(const Audio& audio, const Vector3& position) = 0;
        virtual void SetPitch(const Audio& audio, float pitch) = 0;
        virtual void SetPlaybackSpeed(const Audio& audio, float speed) = 0;
        virtual void SetLooping(const Audio& audio, bool loop) = 0;
        virtual void SetVolume(const Audio& audio, float volume) = 0;
    };
}
