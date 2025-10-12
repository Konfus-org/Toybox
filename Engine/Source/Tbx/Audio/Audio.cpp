#include "Tbx/PCH.h"
#include "Tbx/Audio/Audio.h"

namespace Tbx
{
    Audio::Audio(SampleData data, AudioFormat format)
        : Data(std::move(data))
        , Format(std::move(format))
    {
    }

    void Audio::Play()
    {
        _isPlaying = true;
    }

    void Audio::Stop()
    {
        _isPlaying = false;
    }

    bool Audio::IsPlaying() const
    {
        return _isPlaying;
    }

}
