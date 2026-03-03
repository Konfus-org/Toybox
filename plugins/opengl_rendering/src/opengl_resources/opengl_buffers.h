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
        /// <summary>Creates an OpenGL vertex buffer object.</summary>
        /// <remarks>Purpose: Allocates a GPU buffer handle for vertex data.
        /// Ownership: Owns the created OpenGL handle.
        /// Thread Safety: Construct on the render thread.</remarks>
        OpenGlVertexBuffer();

        /// <summary>Destroys the OpenGL vertex buffer object.</summary>
        /// <remarks>Purpose: Releases the owned GPU buffer handle.
        /// Ownership: Releases only resources owned by this instance.
        /// Thread Safety: Destroy on the render thread.</remarks>
        ~OpenGlVertexBuffer() noexcept override;

        /// <summary>Uploads vertex data and configures VAO attributes.</summary>
        /// <remarks>Purpose: Copies vertex data to GPU and sets VAO attribute formats/bindings.
        /// Ownership: Copies CPU-side data; caller retains CPU ownership.
        /// Thread Safety: Call only on the render thread.</remarks>
        void upload(tbx::uint32 vertex_array_id, const VertexBuffer& buffer);

        /// <summary>Binds the vertex buffer to GL_ARRAY_BUFFER.</summary>
        /// <remarks>Purpose: Provides compatibility binding for non-DSA code paths.
        /// Ownership: Does not transfer ownership of the buffer handle.
        /// Thread Safety: Call only on the render thread.</remarks>
        void bind() override;

        /// <summary>Unbinds any vertex buffer from GL_ARRAY_BUFFER.</summary>
        /// <remarks>Purpose: Clears compatibility binding state.
        /// Ownership: Does not transfer ownership of any resource.
        /// Thread Safety: Call only on the render thread.</remarks>
        void unbind() override;

        /// <summary>Returns uploaded vertex element count.</summary>
        /// <remarks>Purpose: Reports the currently uploaded vertex scalar count.
        /// Ownership: Returns a value; no ownership transfer.
        /// Thread Safety: Safe to read on render thread; external sync required
        /// otherwise.</remarks>
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
        /// <summary>Creates an OpenGL index buffer object.</summary>
        /// <remarks>Purpose: Allocates a GPU buffer handle for index data.
        /// Ownership: Owns the created OpenGL handle.
        /// Thread Safety: Construct on the render thread.</remarks>
        OpenGlIndexBuffer();

        /// <summary>Destroys the OpenGL index buffer object.</summary>
        /// <remarks>Purpose: Releases the owned GPU buffer handle.
        /// Ownership: Releases only resources owned by this instance.
        /// Thread Safety: Destroy on the render thread.</remarks>
        ~OpenGlIndexBuffer() noexcept override;

        /// <summary>Uploads index data and associates it with a VAO.</summary>
        /// <remarks>Purpose: Copies index data to GPU and assigns it as the VAO element buffer.
        /// Ownership: Copies CPU-side data; caller retains CPU ownership.
        /// Thread Safety: Call only on the render thread.</remarks>
        void upload(tbx::uint32 vertex_array_id, const IndexBuffer& buffer);

        /// <summary>Binds the index buffer to GL_ELEMENT_ARRAY_BUFFER.</summary>
        /// <remarks>Purpose: Provides compatibility binding for non-DSA code paths.
        /// Ownership: Does not transfer ownership of the buffer handle.
        /// Thread Safety: Call only on the render thread.</remarks>
        void bind() override;

        /// <summary>Unbinds any index buffer from GL_ELEMENT_ARRAY_BUFFER.</summary>
        /// <remarks>Purpose: Clears compatibility binding state.
        /// Ownership: Does not transfer ownership of any resource.
        /// Thread Safety: Call only on the render thread.</remarks>
        void unbind() override;

        /// <summary>Returns uploaded index count.</summary>
        /// <remarks>Purpose: Reports the currently uploaded index count.
        /// Ownership: Returns a value; no ownership transfer.
        /// Thread Safety: Safe to read on render thread; external sync required
        /// otherwise.</remarks>
        tbx::uint32 get_count() const;

      private:
        tbx::uint32 _buffer_id = 0;
        tbx::uint32 _count = 0;
    };
}
