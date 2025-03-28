#include "Tbx/App/PCH.h"
#include "Tbx/App/Time/DeltaTime.h"

namespace Tbx::Time
{
    float DeltaTime::_valueInSeconds;
    std::chrono::high_resolution_clock::time_point DeltaTime::_lastFrameTime;

    EXPORT float DeltaTime::Seconds()
    {
        return _valueInSeconds;
    }

    EXPORT float DeltaTime::Milliseconds()
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