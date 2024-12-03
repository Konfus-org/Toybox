#pragma once
#include "ModuleDefs.h"

namespace Toybox 
{
    // TODO: wrap our default modules in this and make diff system for handling errors...
    class DefaultWindowModule : public WindowModule
    {
        IWindow* OpenNewWindow(const std::string& name, WindowMode mode, Size size) override
        {
            return nullptr;
        }

        void DestroyWindow(IWindow* windowToDestroy) override
        {
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
        IInputHandler* CreateInputHandler(void* mainNativeWindow) override
        {
            return nullptr;
        }

        void DestroyInputHandler(IInputHandler* handlerToDestroy)  override
        {
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

        void DestroyLogger(ILogger* loggerToDestroy) override
        {
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