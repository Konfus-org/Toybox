#pragma once
#include "IBindable.h"
#include <Tbx/Graphics/Mesh.h>
#include <Tbx/Math/Int.h>

namespace OpenGLRendering
{
    class OpenGLVertexBuffer final: public IBindable
    {
    public:
        OpenGLVertexBuffer();
        ~OpenGLVertexBuffer() override;

        void Bind() const override;
        void Unbind() const override;

        void Upload(const Tbx::VertexBuffer& buffer);
        void AddAttribute(Tbx::uint index, Tbx::uint32 size, Tbx::uint32 type, Tbx::uint32 stride, Tbx::uint32 offset, bool normalized) const;

        Tbx::uint32 GetCount() const { return _count; }

    private:
        Tbx::uint32 _vertBufferGLId = -1;
        Tbx::uint32 _count = 0;
    };

    class OpenGLIndexBuffer final : public IBindable
    {
    public:
        OpenGLIndexBuffer();
        ~OpenGLIndexBuffer() override;

        void Bind() const override;
        void Unbind() const override;

        void Upload(const Tbx::IndexBuffer& buffer);

        Tbx::uint32 GetCount() const { return _count; }

    private:
        Tbx::uint32 _indexBuffGLId = -1;
        Tbx::uint32 _count = 0;
    };
}
