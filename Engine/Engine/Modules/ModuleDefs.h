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
    };

    class InputModule : public Module
    {
    public:
        virtual IInputHandler* CreateInputHandler() = 0;
    };

    class LoggerModule : public Module
    {
    public:
        virtual ILogger* CreateLogger(const std::string& name) = 0;
    };

    class DefaultWindowModule : public WindowModule
    {
        IWindow* OpenNewWindow(const std::string& name, WindowMode mode, Size size) override
        {
            return nullptr;
        }

        const std::string GetName() const override
        {
            return "Default (Error Loading Modules!)";
        }

        const std::string GetAuthor() const override
        {
            return "Jeremy Hummel";
        }

        const int GetVersion() const override
        {
            return 0;
        }
    };

    class DefaultInputModule : public InputModule
    {
        IInputHandler* CreateInputHandler() override
        {
            return nullptr;
        }

        const std::string GetName() const override
        {
            return "Default (Error Loading Modules!)";
        }

        const std::string GetAuthor() const override
        {
            return "Jeremy Hummel";
        }

        const int GetVersion() const override
        {
            return 0;
        }
    };

    class DefaultLoggerModule : public LoggerModule
    {
        ILogger* CreateLogger(const std::string& name) override
        {
            return nullptr;
        }

        const std::string GetName() const override
        {
            return "Default (Error Loading Modules!)";
        }

        const std::string GetAuthor() const override
        {
            return "Jeremy Hummel";
        }

        const int GetVersion() const override
        {
            return 0;
        }
    };
}