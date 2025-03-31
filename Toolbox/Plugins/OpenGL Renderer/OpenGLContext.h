#pragma once
#include <Tbx/App/Windowing/IWindow.h>
#include <GLFW/glfw3.h>

namespace OpenGLRendering
{
    class OpenGLContext
    {
    public:
        OpenGLContext() = default;
        ~OpenGLContext() = default;

        void Set(const std::weak_ptr<Tbx::IWindow>& windowToRenderInto);

        void SwapBuffers();
        void SetSwapInterval(const int& interval) const;
        GLFWwindow* GetRenderSurface();

    private:
        GLFWwindow* _windowToRenderTo = nullptr;
    };
}

