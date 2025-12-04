#pragma once
#include "OpenGLBuffers.h"
#include <Tbx/Graphics/Vertex.h>
#include <Tbx/Graphics/GraphicsResources.h>

namespace Tbx::Plugins::OpenGLRendering
{
    class OpenGLMesh final : public MeshResource
    {
    public:
        OpenGLMesh(const Mesh& mesh);
        ~OpenGLMesh() override;

        void Activate() override;
        void Release() override;

        void Draw() override;
        void SetVertexBuffer(const VertexBuffer& buffer) override;
        void SetIndexBuffer(const IndexBuffer& buffer) override;

    private:
        OpenGLVertexBuffer _vertexBuffer;
        OpenGLIndexBuffer _indexBuffer;
    };
}

