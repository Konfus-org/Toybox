#pragma once
#include "Windowing/IWindow.h"
#include "Input/IInputHandler.h"
#include "Debug/Logging/ILogger.h"
#include "ModuleAPI.h"

namespace Toybox::Modules
{
    class WindowModule : public Module
    {
    public:
        virtual Windowing::IWindow* Create() = 0;
    };

    class InputModule : public Module
    {
    public:
        virtual Input::IInputHandler* Create() = 0;
    };

    class LoggerModule : public Module
    {
    public:
        virtual Debug::ILogger* Create() = 0;
    };

    class DefaultWindowModule : public WindowModule
    {
        Windowing::IWindow* Create() override
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
        Input::IInputHandler* Create() override
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
        Debug::ILogger* Create() override
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