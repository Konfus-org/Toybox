#pragma once
#include <Core.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace OpenGLRendering
{
    class OpenGLVertexBuffer
    {
    public:
        OpenGLVertexBuffer();
        ~OpenGLVertexBuffer();

        void SetData(const Toybox::VertexBuffer& vertices);
        void Bind() const;
        void Unbind() const;
        void AddAttribute(Toybox::uint index, Toybox::uint size, Toybox::uint type, Toybox::uint stride, bool normalized) const;

        inline Toybox::uint32 GetCount() const { return _count; }

    private:
        Toybox::uint32 _rendererId;
        Toybox::uint32 _count;
    };

    class OpenGLIndexBuffer
    {
    public:
        OpenGLIndexBuffer();
        ~OpenGLIndexBuffer();

        void SetData(const Toybox::IndexBuffer& indices);
        void Bind() const;
        void Unbind() const;

        inline Toybox::uint32 GetCount() const { return _count; }

    private:
        Toybox::uint32 _rendererId;
        Toybox::uint32 _count;
    };
}
