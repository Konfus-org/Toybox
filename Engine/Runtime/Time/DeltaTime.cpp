#include "DeltaTime.h"

namespace Tbx::Time
{
    void DeltaTime::Update()
    {
        auto currentTime = std::chrono::high_resolution_clock::now();
        auto timeSinceLastFrame = std::chrono::duration_cast<std::chrono::duration<float>>(currentTime - _lastFrameTime).count();
        _valueInSeconds = timeSinceLastFrame;
        _lastFrameTime = currentTime;
    }
}