#include "GLFWWindowFactory.h"
#include "GLFWWindow.h"
#include <GLFW/glfw3.h>

namespace GLFWWindowing
{
    void DeleteWindow(GLFWWindow* window)
    {
        delete window;
    }

    GLFWWindow* CreateNewWindow()
    {
        return new GLFWWindow();
    }

    std::shared_ptr<Tbx::IWindow> GLFWWindowFactory::Create(const std::string& title, const Tbx::Size& size)
    {
        // Need to do it this way because THIS dll owns this pointer and needs to delete it!
        const auto& window = std::shared_ptr<GLFWWindow>(CreateNewWindow(), [](GLFWWindow* windowToDelete)
        { 
            DeleteWindow(windowToDelete); 
        });

        // We set title and size, but DO NOT open, that is done by the user of the windowing plugin
        // Our job is simply to create the window
        window->SetTitle(title);
        window->SetSize(size);

        return window;
    }
}