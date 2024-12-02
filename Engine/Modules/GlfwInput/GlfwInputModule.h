#pragma once
#include <Toybox.h>

namespace GlfwInput
{
    class GlfwInputModule : public Toybox::Modules::InputModule
    {
    public:
        Toybox::Input::IInputHandler* Create() override;
        const std::string GetName() const override;
        const std::string GetAuthor() const override;
        const int GetVersion() const override;
    };
}

extern "C" TBX_MODULE_API Toybox::Modules::InputModule* Load();
