#pragma once
#include <Toybox.h>

namespace GlfwWindowing
{
    class GlfwWindowModule : public Toybox::WindowModule
    {
    public:
        Toybox::IWindow* OpenNewWindow(const std::string& name, Toybox::WindowMode mode, Toybox::Size size) override;
        const std::string GetName() const override;
        const std::string GetAuthor() const override;
        const int GetVersion() const override;
    };
}

extern "C" TBX_MODULE_API Toybox::WindowModule* Load();