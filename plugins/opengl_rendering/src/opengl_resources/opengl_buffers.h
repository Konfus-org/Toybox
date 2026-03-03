#pragma once
#include "opengl_resource.h"
#include "tbx/common/int.h"
#include "tbx/graphics/mesh.h"
#include "tbx/graphics/vertex.h"

namespace opengl_rendering
{
    class OpenGlVertexBuffer final : public IOpenGlResource
    {
      public:
        OpenGlVertexBuffer();
        ~OpenGlVertexBuffer() noexcept override;

        void upload(const VertexBuffer& buffer);
        void bind() override;
        void unbind() override;

        tbx::uint32 get_count() const;

      private:
        tbx::uint32 _buffer_id = 0;
        tbx::uint32 _count = 0;
    };

    class OpenGlIndexBuffer final : public IOpenGlResource
    {
      public:
        OpenGlIndexBuffer();
        ~OpenGlIndexBuffer() noexcept override;

        void upload(const IndexBuffer& buffer);
        void bind() override;
        void unbind() override;

        tbx::uint32 get_count() const;

      private:
        tbx::uint32 _buffer_id = 0;
        tbx::uint32 _count = 0;
    };
}
