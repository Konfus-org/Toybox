#include "GLFWWindowFactory.h"
#include "GLFWWindow.h"
#include <GLFW/glfw3.h>

namespace GLFWWindowing
{
    std::shared_ptr<Tbx::IWindow> GLFWWindowFactory::Create(const std::string& title, const Tbx::Size& size)
    {
        const auto& window = std::make_shared<GLFWWindow>();
        window->SetTitle(title);
        window->SetSize(size);
        return window;
    }
}