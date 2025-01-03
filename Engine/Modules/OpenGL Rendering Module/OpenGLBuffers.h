#pragma once
#include <Core.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace OpenGLRendering
{
    class IBindable
    {
    public:
        virtual ~IBindable() = default;

        virtual void Bind() const = 0;
        virtual void Unbind() const = 0;
    };

    class OpenGLVertexBuffer : public IBindable
    {
    public:
        OpenGLVertexBuffer();
        ~OpenGLVertexBuffer() override;

        void SetData(const Toybox::VertexBuffer& vertices);
        void AddAttribute(const Toybox::uint& index, const Toybox::uint& size, const Toybox::uint& type, const Toybox::uint& stride, const Toybox::uint& offset, const bool& normalized) const;

        void Bind() const override;
        void Unbind() const override;

        Toybox::uint32 GetCount() const { return _count; }

    private:
        Toybox::uint32 _rendererId;
        Toybox::uint32 _count;
    };

    class OpenGLIndexBuffer : public IBindable
    {
    public:
        OpenGLIndexBuffer();
        ~OpenGLIndexBuffer() override;

        void SetData(const Toybox::IndexBuffer& indices);
        void Bind() const override;
        void Unbind() const override;

        Toybox::uint32 GetCount() const { return _count; }

    private:
        Toybox::uint32 _rendererId;
        Toybox::uint32 _count;
    };

    class OpenGLVertexArray : public IBindable
    {
    public:
        OpenGLVertexArray();
        ~OpenGLVertexArray() override;

        void Bind() const override;
        void Unbind() const override;

        void AddVertexBuffer(const Toybox::VertexBuffer& buffer);
        void SetIndexBuffer(const Toybox::IndexBuffer& buffer);

        const std::vector<OpenGLVertexBuffer>& GetVertexBuffers() const { return _vertexBuffers; }
        const OpenGLIndexBuffer& GetIndexBuffer() const { return _indexBuffer; }
        Toybox::uint32 GetIndexCount() const { return _indexBuffer.GetCount(); }

        void Clear() { _vertexBuffers.clear(); }

    private:
        std::vector<OpenGLVertexBuffer> _vertexBuffers;
        OpenGLIndexBuffer _indexBuffer;
        Toybox::uint32 _rendererId;
    };
}
