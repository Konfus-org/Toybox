#pragma once
#include "GlfwWindow.h"
#include <Toybox.h>

namespace GlfwWindowing
{
    class GlfwWindowModule : public Toybox::Modules::WindowModule
    {
    public:
        Toybox::Windowing::IWindow* Create() override
        {
            return new GlfwWindow();
        }
    };
}

extern Toybox::Modules::WindowModule* Load()
{
    return new GlfwWindowing::GlfwWindowModule();
}