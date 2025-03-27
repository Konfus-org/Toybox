#pragma once
#include "ToolboxAPI.h"
#include <chrono>

namespace Tbx::Time
{
    class DeltaTime
    {
    public:
        // Gets delta time in seconds
        TBX_API static float Seconds();

        // Gets delta time in milliseconds
        TBX_API static float Milliseconds();

        // Updates delta time
        // Should be called once per frame
        TBX_API static void Update();

    private:
        static float _valueInSeconds;
        static std::chrono::high_resolution_clock::time_point _lastFrameTime;

        friend class App;
    };
}