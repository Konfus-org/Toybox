#pragma once
#include <Toybox.h>

namespace GlfwWindowing
{
    class GlfwWindowModule : public Toybox::Modules::WindowModule
    {
    public:
        Toybox::Windowing::IWindow* Create() override;
        const std::string GetName() const override;
        const std::string GetAuthor() const override;
        const int GetVersion() const override;
    };
}

extern "C" TBX_MODULE_API Toybox::Modules::WindowModule* Load();