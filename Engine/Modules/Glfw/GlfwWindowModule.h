#pragma once
#include <Toybox.h>

namespace GlfwWindowing
{
    class GlfwWindowModule : public Toybox::WindowModule
    {
    public:
        GlfwWindowModule() = default;
        ~GlfwWindowModule() final = default;

        Toybox::IWindow* OpenNewWindow(const std::string& name, Toybox::WindowMode mode, Toybox::Size size) override;
        void DestroyWindow(Toybox::IWindow* windowToDestroy) override;
        std::string GetName() const override;
        std::string GetAuthor() const override;
        int GetVersion() const override;
    };
}