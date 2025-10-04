#pragma once
#include "IBindable.h"
#include "OpenGLBuffers.h"
#include <Tbx/Graphics/Vertex.h>
#include <Tbx/Math/Int.h>

namespace OpenGLRendering
{
    class OpenGLMesh final : public IBindable
    {
    public:
        OpenGLMesh();
        ~OpenGLMesh() override;

        void Bind() const override;
        void Unbind() const override;

        void UploadVertexBuffer(const Tbx::VertexBuffer& buffer);
        void UploadIndexBuffer(const std::vector<Tbx::uint32>& buffer);

        const OpenGLVertexBuffer& GetVertexBuffer() const { return _vertexBuffer; }
        const OpenGLIndexBuffer& GetIndexBuffer() const { return _indexBuffer; }

    private:
        OpenGLVertexBuffer _vertexBuffer;
        OpenGLIndexBuffer _indexBuffer;
        Tbx::uint32 _rendererId = -1;
    };
}

