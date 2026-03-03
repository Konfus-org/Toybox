#pragma once
#include "opengl_resource.h"
#include "tbx/common/int.h"
#include "tbx/graphics/mesh.h"
#include "tbx/graphics/vertex.h"

namespace opengl_rendering
{
    /// <summary>OpenGL vertex buffer resource.</summary>
    /// <remarks>Purpose: Owns vertex data uploaded to GPU memory and configures VAO attribute
    /// state. Ownership: Owns the OpenGL buffer handle and never owns caller-provided CPU data.
    /// Thread Safety: Not thread-safe; call from the render thread.</remarks>
    class OpenGlVertexBuffer final : public IOpenGlResource
    {
      public:
        OpenGlVertexBuffer();
        OpenGlVertexBuffer(const OpenGlVertexBuffer&) = delete;
        OpenGlVertexBuffer& operator=(const OpenGlVertexBuffer&) = delete;
        OpenGlVertexBuffer(OpenGlVertexBuffer&& other) noexcept;
        OpenGlVertexBuffer& operator=(OpenGlVertexBuffer&& other) noexcept;
        ~OpenGlVertexBuffer() noexcept override;

        void upload(tbx::uint32 vertex_array_id, const tbx::VertexBuffer& buffer);
        void bind() override;
        void unbind() override;

        tbx::uint32 get_count() const;

      private:
        tbx::uint32 _buffer_id = 0;
        tbx::uint32 _count = 0;
    };

    /// <summary>OpenGL index buffer resource.</summary>
    /// <remarks>Purpose: Owns index data uploaded to GPU memory and binds it to a VAO.
    /// Ownership: Owns the OpenGL buffer handle and never owns caller-provided CPU data.
    /// Thread Safety: Not thread-safe; call from the render thread.</remarks>
    class OpenGlIndexBuffer final : public IOpenGlResource
    {
      public:
        OpenGlIndexBuffer();
        OpenGlIndexBuffer(const OpenGlIndexBuffer&) = delete;
        OpenGlIndexBuffer& operator=(const OpenGlIndexBuffer&) = delete;
        OpenGlIndexBuffer(OpenGlIndexBuffer&& other) noexcept;
        OpenGlIndexBuffer& operator=(OpenGlIndexBuffer&& other) noexcept;
        ~OpenGlIndexBuffer() noexcept override;

        void upload(tbx::uint32 vertex_array_id, const tbx::IndexBuffer& buffer);
        void bind() override;
        void unbind() override;

        tbx::uint32 get_count() const;

      private:
        tbx::uint32 _buffer_id = 0;
        tbx::uint32 _count = 0;
    };
}
