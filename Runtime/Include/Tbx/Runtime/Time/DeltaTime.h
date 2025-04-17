#pragma once
#include <Tbx/Core/DllExport.h>
#include <chrono>

namespace Tbx::Time
{
    class DeltaTime
    {
    public:
        // Gets delta time in seconds
        EXPORT static float Seconds();

        // Gets delta time in milliseconds
        EXPORT static float Milliseconds();

    private:
        friend class App;

        // Updates delta time
        // Should be called once per frame
        static void Update();

        static float _valueInSeconds;
        static std::chrono::high_resolution_clock::time_point _lastFrameTime;
    };
}