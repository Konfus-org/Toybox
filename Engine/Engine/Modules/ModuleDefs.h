#pragma once
#include "Windowing/IWindow.h"
#include "Input/IInputHandler.h"
#include "Debug/ILogger.h"
#include "ModuleAPI.h"

namespace Toybox
{
    class RendererModule : public Module
    {
    public:
        RendererModule() = default;
        virtual ~RendererModule() = default;

        virtual IWindow* CreateRenderer(const std::string& name, WindowMode mode, Size size) = 0;
        virtual void DestroyRenderer(IWindow* windowToDestroy) = 0;
    };

    class WindowModule : public Module
    {
    public:
        WindowModule() = default;
        virtual ~WindowModule() = default;

        virtual IWindow* CreateNewWindow(const std::string& name, WindowMode mode, Size size) = 0;
        virtual void DestroyWindow(IWindow* windowToDestroy) = 0;
    };

    class InputModule : public Module
    {
    public:
        InputModule() = default;
        virtual ~InputModule() = default;

        virtual IInputHandler* CreateInputHandler(std::any mainNativeWindow) = 0;
        virtual void DestroyInputHandler(IInputHandler* handlerToDestroy) = 0;
    };

    class LoggerModule : public Module
    {
    public:
        LoggerModule() = default;
        virtual ~LoggerModule() = default;

        virtual ILogger* CreateLogger(const std::string& name) = 0;
        virtual void DestroyLogger(ILogger* loggerToDestroy) = 0;
    };
}