#include "DeltaTime.h"

namespace Tbx::Time
{
    float DeltaTime::_valueInSeconds;
    std::chrono::high_resolution_clock::time_point DeltaTime::_lastFrameTime;

    TBX_API float DeltaTime::Seconds()
    {
        return _valueInSeconds;
    }

    TBX_API float DeltaTime::Milliseconds()
    {
        return _valueInSeconds * 1000.0f;
    }

    void DeltaTime::Update()
    {
        auto currentTime = std::chrono::high_resolution_clock::now();
        auto timeSinceLastFrame = std::chrono::duration_cast<std::chrono::duration<float>>(currentTime - _lastFrameTime).count();
        _valueInSeconds = timeSinceLastFrame;
        _lastFrameTime = currentTime;
    }
}