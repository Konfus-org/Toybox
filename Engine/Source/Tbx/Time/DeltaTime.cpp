#include "Tbx/PCH.h"
#include "Tbx/Time/DeltaTime.h"

namespace Tbx
{
    float DeltaTime::_valueInSeconds;
    std::chrono::high_resolution_clock::time_point DeltaTime::_lastFrameTime;

    float DeltaTime::InSeconds()
    {
        return _valueInSeconds;
    }

    float DeltaTime::InMilliseconds()
    {
        return _valueInSeconds * 1000.0f;
    }

    void DeltaTime::Update()
    {
        auto currentTime = std::chrono::high_resolution_clock::now();

        if (_lastFrameTime.time_since_epoch().count() == 0)
        {
            _lastFrameTime = currentTime;
            _valueInSeconds = 0.0f;
            return;
        }

        auto timeSinceLastFrame = std::chrono::duration_cast<std::chrono::duration<float>>(currentTime - _lastFrameTime).count();
        _valueInSeconds = timeSinceLastFrame;
        _lastFrameTime = currentTime;
    }

    void DeltaTime::Set(float seconds)
    {
        _valueInSeconds = seconds;
    }
}