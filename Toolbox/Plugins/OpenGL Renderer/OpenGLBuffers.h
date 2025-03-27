#pragma once
#include "IBindable.h"
#include <TbxCore.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace OpenGLRendering
{
    class OpenGLVertexBuffer : public IBindable
    {
    public:
        OpenGLVertexBuffer();
        ~OpenGLVertexBuffer() final;

        void SetData(const Tbx::VertexBuffer& vertices);
        void AddAttribute(const Tbx::uint& index, const Tbx::uint& size, const Tbx::uint& type, const Tbx::uint& stride, const Tbx::uint& offset, const bool& normalized) const;

        void Bind() const final;
        void Unbind() const final;

        Tbx::uint32 GetCount() const { return _count; }

    private:
        Tbx::uint32 _rendererId = -1;
        Tbx::uint32 _count = 0;
    };

    class OpenGLIndexBuffer : public IBindable
    {
    public:
        OpenGLIndexBuffer();
        ~OpenGLIndexBuffer() final;

        void SetData(const Tbx::IndexBuffer& indices);
        void Bind() const final;
        void Unbind() const final;

        Tbx::uint32 GetCount() const { return _count; }

    private:
        Tbx::uint32 _rendererId = -1;
        Tbx::uint32 _count = 0;
    };

    class OpenGLVertexArray : public IBindable
    {
    public:
        OpenGLVertexArray();
        ~OpenGLVertexArray() final;

        void Bind() const final;
        void Unbind() const final;

        void AddVertexBuffer(const Tbx::VertexBuffer& buffer);
        void SetIndexBuffer(const Tbx::IndexBuffer& buffer);

        const std::vector<OpenGLVertexBuffer>& GetVertexBuffers() const { return _vertexBuffers; }
        const OpenGLIndexBuffer& GetIndexBuffer() const { return _indexBuffer; }
        Tbx::uint32 GetIndexCount() const { return _indexBuffer.GetCount(); }

        void Clear() { _vertexBuffers.clear(); }

    private:
        std::vector<OpenGLVertexBuffer> _vertexBuffers;
        OpenGLIndexBuffer _indexBuffer;
        Tbx::uint32 _rendererId = -1;
    };
}
