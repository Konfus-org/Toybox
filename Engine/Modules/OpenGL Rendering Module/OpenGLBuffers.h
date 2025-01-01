#pragma once
#include <Core.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace OpenGLRendering
{
    static GLenum ShaderDataTypeToOpenGLType(const Toybox::ShaderDataType& type)
    {
        using enum Toybox::ShaderDataType;
        switch (type)
        {
            case None:     return GL_NONE;
            case Float:    return GL_FLOAT;
            case Float2:   return GL_FLOAT;
            case Float3:   return GL_FLOAT;
            case Float4:   return GL_FLOAT;
            case Mat3:     return GL_FLOAT;
            case Mat4:     return GL_FLOAT;
            case Int:      return GL_INT;
            case Int2:     return GL_INT;
            case Int3:     return GL_INT;
            case Int4:     return GL_INT;
            case Bool:     return GL_BOOL;
        }

        TBX_ASSERT(false, "Couln not convert to OpenGL type from ShaderDataType, given unknown ShaderDataType!");
        return GL_NONE;
    }

    class OpenGLVertexBuffer
    {
    public:
        OpenGLVertexBuffer();
        ~OpenGLVertexBuffer();

        void SetData(const Toybox::VertexBuffer& vertices);
        void SetLayout(const Toybox::BufferLayout& layout) const;

        void AddAttribute(const Toybox::uint& index, const Toybox::uint& size, const Toybox::uint& type, const Toybox::uint& stride, const Toybox::uint& offset, const bool& normalized) const;

        void Bind() const;
        void Unbind() const;

        Toybox::uint32 GetCount() const { return _count; }

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

        Toybox::uint32 GetCount() const { return _count; }

    private:
        Toybox::uint32 _rendererId;
        Toybox::uint32 _count;
    };
}
