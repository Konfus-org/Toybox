#pragma once
#include <TbxCore.h>
#include <GLFW/glfw3.h>

namespace OpenGLRendering
{
    class OpenGLContext
    {
    public:
        explicit OpenGLContext(const std::weak_ptr<Tbx::IWindow>& windowToRenderInto);
        ~OpenGLContext() = default;

        // TODO: implement
        ////void SetData(const std::any& data, size_t size);
        ////void Bind() const;
        ////void Unbind() const ;

        void SwapBuffers();
        void SetSwapInterval(const int& interval) const;
        GLFWwindow* GetRenderSurface();

    private:
        GLFWwindow* _windowToRenderTo = nullptr;
    };
}

