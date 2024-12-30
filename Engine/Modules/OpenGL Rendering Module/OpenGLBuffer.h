#pragma once
#include <Core.h>
#include <GLFW/glfw3.h>

namespace OpenGLRendering
{
    class OpenGLBuffer
    {
    public:
        explicit OpenGLBuffer(const std::weak_ptr<Toybox::IWindow>& windowToRenderInto);
        ~OpenGLBuffer() = default;

        // TODO: implement
        ////void SetData(const std::any& data, size_t size);
        ////void Bind() const;
        ////void Unbind() const ;

        void Swap();
        void SetSwapInterval(const int& interval) const;

    private:
        GLFWwindow* _windowToRenderTo = nullptr;
    };
}

