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
        TBX_API static float Seconds();
        // Gets delta time in milliseconds
        TBX_API static float Milliseconds();

    private:
        static float _valueInSeconds;
        static std::chrono::high_resolution_clock::time_point _lastFrameTime;

        static void Update();

        friend class App;
    };
}