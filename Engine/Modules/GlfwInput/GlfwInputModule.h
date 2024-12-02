#pragma once
#include <Toybox.h>

namespace GlfwInput
{
    class GlfwInputModule : public Toybox::InputModule
    {
    public:
        Toybox::IInputHandler* CreateInputHandler() override;
        const std::string GetName() const override;
        const std::string GetAuthor() const override;
        const int GetVersion() const override;
    };
}

extern "C" TBX_MODULE_API Toybox::InputModule* Load();
