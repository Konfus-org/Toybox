#pragma once
#include "GlfwWindow.h"
#include <Toybox.h>

namespace GlfwWindowing
{
    TBX_MODULE_API class GlfwWindowModule : public Toybox::Modules::WindowModule
    {
    public:
        Toybox::Windowing::IWindow* Create() override
        {
            return new GlfwWindow();
        }

        const std::string GetName() const override
        {
            return "Glfw Windowing";
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

extern Toybox::Modules::WindowModule* Load()
{
    return new GlfwWindowing::GlfwWindowModule();
}