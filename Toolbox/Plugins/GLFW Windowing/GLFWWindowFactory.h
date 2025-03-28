#pragma once
#include <Tbx/App/Windowing/IWindowFactory.h>

namespace GLFWWindowing
{
    class GLFWWindowFactory : public Tbx::IWindowFactory
    {
    public:
        std::shared_ptr<Tbx::IWindow> Create(const std::string& title, const Tbx::Size& size) override;
    };
}