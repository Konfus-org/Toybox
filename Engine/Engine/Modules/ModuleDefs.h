#pragma once
#include "Windowing/IWindow.h"
#include "Input/IInputHandler.h"
#include "Debug/Logging/ILogger.h"

namespace Toybox::Modules
{
    class WindowModule
    {
    public:
        virtual Windowing::IWindow* Create() = 0;
    };

    class InputModule
    {
    public:
        virtual Input::IInputHandler* Create() = 0;
    };

    class LoggerModule
    {
    public:
        virtual Debug::ILogger* Create() = 0;
    };
}