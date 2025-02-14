#pragma once
#include <TbxCore.h>
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

        void SetData(const Tbx::VertexBuffer& vertices);
        void AddAttribute(const Tbx::uint& index, const Tbx::uint& size, const Tbx::uint& type, const Tbx::uint& stride, const Tbx::uint& offset, const bool& normalized) const;

        void Bind() const override;
        void Unbind() const override;

        Tbx::uint32 GetCount() const { return _count; }

    private:
        Tbx::uint32 _rendererId;
        Tbx::uint32 _count;
    };

    class OpenGLIndexBuffer : public IBindable
    {
    public:
        OpenGLIndexBuffer();
        ~OpenGLIndexBuffer() override;

        void SetData(const Tbx::IndexBuffer& indices);
        void Bind() const override;
        void Unbind() const override;

        Tbx::uint32 GetCount() const { return _count; }

    private:
        Tbx::uint32 _rendererId;
        Tbx::uint32 _count;
    };

    class OpenGLVertexArray : public IBindable
    {
    public:
        OpenGLVertexArray();
        ~OpenGLVertexArray() override;

        void Bind() const override;
        void Unbind() const override;

        void AddVertexBuffer(const Tbx::VertexBuffer& buffer);
        void SetIndexBuffer(const Tbx::IndexBuffer& buffer);

        const std::vector<OpenGLVertexBuffer>& GetVertexBuffers() const { return _vertexBuffers; }
        const OpenGLIndexBuffer& GetIndexBuffer() const { return _indexBuffer; }
        Tbx::uint32 GetIndexCount() const { return _indexBuffer.GetCount(); }

        void Clear() { _vertexBuffers.clear(); }

    private:
        std::vector<OpenGLVertexBuffer> _vertexBuffers;
        OpenGLIndexBuffer _indexBuffer;
        Tbx::uint32 _rendererId;
    };
}
