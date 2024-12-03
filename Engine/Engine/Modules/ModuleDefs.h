#pragma once
#include "Windowing/IWindow.h"
#include "Input/IInputHandler.h"
#include "Debug/ILogger.h"
#include "ModuleAPI.h"

namespace Toybox
{
    class WindowModule : public Module
    {
    public:
        virtual IWindow* OpenNewWindow(const std::string& name, WindowMode mode, Size size) = 0;
        virtual void DestroyWindow(IWindow* windowToDestroy) = 0;
    };

    class InputModule : public Module
    {
    public:
        virtual IInputHandler* CreateInputHandler(void* mainNativeWindow) = 0;
        virtual void DestroyInputHandler(IInputHandler* handlerToDestroy) = 0;
    };

    class LoggerModule : public Module
    {
    public:
        virtual ILogger* CreateLogger(const std::string& name) = 0;
        virtual void DestroyLogger(ILogger* loggerToDestroy) = 0;
    };
}