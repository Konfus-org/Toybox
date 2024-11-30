#pragma once
#include "GlfwInputHandler.h"
#include <Toybox.h>

namespace GlfwInput
{
    TBX_MODULE_API class GlfwInputModule : public Toybox::Modules::InputModule
    {
    public:
        Toybox::Input::IInputHandler* Create() override
        {
            return new GlfwInputHandler();
        }

        const std::string GetName() const override
        {
            return "Glfw Input";
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

extern Toybox::Modules::InputModule* Load()
{
    return new GlfwInput::GlfwInputModule();
}
