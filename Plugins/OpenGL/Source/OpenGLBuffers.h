#pragma once
#include <Tbx/Graphics/Mesh.h>
#include <Tbx/Math/Int.h>

namespace Tbx::Plugins::OpenGLRendering
{
    class OpenGLVertexBuffer final
    {
    public:
        OpenGLVertexBuffer();
        ~OpenGLVertexBuffer();

        void Bind() const;
        void Unbind() const;

        void Upload(const VertexBuffer& buffer);
        void AddAttribute(uint index, uint32 size, uint32 type, uint32 stride, uint32 offset, bool normalized) const;

        uint32 GetCount() const { return _count; }

    private:
        uint32 _vertBufferGLId = -1;
        uint32 _count = 0;
    };

    class OpenGLIndexBuffer final
    {
    public:
        OpenGLIndexBuffer();
        ~OpenGLIndexBuffer();

        void Bind() const;
        void Unbind() const;

        void Upload(const IndexBuffer& buffer);

        uint32 GetCount() const { return _count; }

    private:
        uint32 _indexBuffGLId = -1;
        uint32 _count = 0;
    };
}
