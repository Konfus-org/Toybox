#pragma once
#include "Application/App.h"
#include <TbxCore.h>
#include <chrono>

namespace Tbx::Time
{
    class DeltaTime
    {
    public:
        // Gets delta time in seconds
        TBX_API static float Seconds() { return _valueInSeconds; }
        // Gets delta time in milliseconds
        TBX_API static float Milliseconds() { return _valueInSeconds * 1000.0f; }

    private:
        static inline float _valueInSeconds;
        static inline std::chrono::high_resolution_clock::time_point _lastFrameTime;

        static void Update();

        friend class App;
    };
}