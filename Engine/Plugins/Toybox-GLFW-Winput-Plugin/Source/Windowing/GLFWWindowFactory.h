#pragma once
#include <Tbx/Application/Windowing/IWindow.h>

namespace GLFWWindowing
{
    class GLFWWindowFactory : public Tbx::IWindowFactory
    {
    public:
        GLFWWindowFactory() = default;
        ~GLFWWindowFactory() override = default;

        std::shared_ptr<Tbx::IWindow> Create(const std::string& title, const Tbx::Size& size) override;
    };
}