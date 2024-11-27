#pragma once
#include "GlfwInputHandler.h"
#include <Toybox.h>

namespace GlfwInput
{
    class GlfwInputModule : public Toybox::Modules::InputModule
    {
    public:
        Toybox::Input::IInputHandler* Create() override
        {
            return new GlfwInputHandler();
        }
    };
}

extern Toybox::Modules::InputModule* Load()
{
    return new GlfwInput::GlfwInputModule();
}
