#pragma once
#include <Core.h>

namespace GlfwWindowing
{
    class GlfwWindowModule : public Toybox::WindowModule
    {
    public:
        GlfwWindowModule() = default;
        ~GlfwWindowModule() final = default;

        Toybox::IWindow* CreateNewWindow(const std::string& name, Toybox::WindowMode mode, Toybox::Size size) override;
        void DestroyWindow(Toybox::IWindow* windowToDestroy) override;
        std::string GetName() const override;
        std::string GetAuthor() const override;
        int GetVersion() const override;
    };
}