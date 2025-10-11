#include "Tbx/PCH.h"
#include "Tbx/Time/DeltaTime.h"

namespace Tbx
{
    float DeltaTime::InSeconds() const
    {
        return Seconds;
    }

    float DeltaTime::InMilliseconds() const
    {
        return Seconds * 1000.0f;
    }

    DeltaClock::DeltaClock()
        : _lastFrameTime()
        , _hasLastFrameTime(false)
    {
    }

    DeltaTime DeltaClock::Tick()
    {
        const auto currentTime = std::chrono::high_resolution_clock::now();

        if (!_hasLastFrameTime)
        {
            _hasLastFrameTime = true;
            _lastFrameTime = currentTime;
            return DeltaTime{};
        }

        const auto deltaSeconds = std::chrono::duration_cast<std::chrono::duration<float>>(currentTime - _lastFrameTime).count();
        _lastFrameTime = currentTime;
        return DeltaTime(deltaSeconds);
    }

    void DeltaClock::Reset()
    {
        _hasLastFrameTime = false;
    }
}
