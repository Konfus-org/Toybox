#pragma once
#include "tbx/common/int.h"
#include "tbx/graphics/mesh.h"
#include "tbx/graphics/vertex.h"

namespace tbx::plugins::openglrendering
{
    /// <summary>Owns an OpenGL vertex buffer object.</summary>
    /// <remarks>Purpose: Uploads vertex data and configures attribute pointers.
    /// Ownership: Owns the OpenGL buffer identifier and releases it on destruction.
    /// Thread Safety: Not thread-safe; use on the render thread.</remarks>
    class OpenGlVertexBuffer final
    {
      public:
        /// <summary>Creates an OpenGL vertex buffer.</summary>
        /// <remarks>Purpose: Allocates the underlying GPU buffer handle.
        /// Ownership: The instance owns the created buffer.
        /// Thread Safety: Construct on the render thread.</remarks>
        OpenGlVertexBuffer();

        /// <summary>Destroys the OpenGL vertex buffer.</summary>
        /// <remarks>Purpose: Releases the GPU buffer handle.
        /// Ownership: The instance owns the buffer being destroyed.
        /// Thread Safety: Destroy on the render thread.</remarks>
        ~OpenGlVertexBuffer();

        /// <summary>Uploads vertex data to the buffer.</summary>
        /// <remarks>Purpose: Copies vertex data into GPU memory.
        /// Ownership: Copies from the provided buffer; caller retains CPU ownership.
        /// Thread Safety: Call only on the render thread.</remarks>
        void upload(const VertexBuffer& buffer);

        /// <summary>Adds a vertex attribute pointer for the currently bound VAO.</summary>
        /// <remarks>Purpose: Configures how vertex data is read by the shader.
        /// Ownership: Does not transfer ownership of any resources.
        /// Thread Safety: Call only on the render thread.</remarks>
        void add_attribute(
            uint32 index,
            uint32 size,
            uint32 type,
            uint32 stride,
            uint32 offset,
            bool normalized) const;

        /// <summary>Binds the vertex buffer to GL_ARRAY_BUFFER.</summary>
        /// <remarks>Purpose: Makes the buffer active for subsequent operations.
        /// Ownership: The instance retains ownership of the buffer.
        /// Thread Safety: Call only on the render thread.</remarks>
        void bind() const;

        /// <summary>Unbinds the vertex buffer from GL_ARRAY_BUFFER.</summary>
        /// <remarks>Purpose: Clears the buffer binding.
        /// Ownership: The instance retains ownership of the buffer.
        /// Thread Safety: Call only on the render thread.</remarks>
        void unbind() const;

        /// <summary>Returns the number of vertices stored in the buffer.</summary>
        /// <remarks>Purpose: Allows draw calls to know the vertex count.
        /// Ownership: Returns a value type; no ownership transfer.
        /// Thread Safety: Safe to call on the render thread.</remarks>
        uint32 get_count() const;

      private:
        uint32 _buffer_id = 0;
        uint32 _count = 0;
    };

    /// <summary>Owns an OpenGL index buffer object.</summary>
    /// <remarks>Purpose: Uploads index data for indexed draw calls.
    /// Ownership: Owns the OpenGL buffer identifier and releases it on destruction.
    /// Thread Safety: Not thread-safe; use on the render thread.</remarks>
    class OpenGlIndexBuffer final
    {
      public:
        /// <summary>Creates an OpenGL index buffer.</summary>
        /// <remarks>Purpose: Allocates the underlying GPU buffer handle.
        /// Ownership: The instance owns the created buffer.
        /// Thread Safety: Construct on the render thread.</remarks>
        OpenGlIndexBuffer();

        /// <summary>Destroys the OpenGL index buffer.</summary>
        /// <remarks>Purpose: Releases the GPU buffer handle.
        /// Ownership: The instance owns the buffer being destroyed.
        /// Thread Safety: Destroy on the render thread.</remarks>
        ~OpenGlIndexBuffer();

        /// <summary>Uploads index data to the buffer.</summary>
        /// <remarks>Purpose: Copies index data into GPU memory.
        /// Ownership: Copies from the provided buffer; caller retains CPU ownership.
        /// Thread Safety: Call only on the render thread.</remarks>
        void upload(const IndexBuffer& buffer);

        /// <summary>Binds the index buffer to GL_ELEMENT_ARRAY_BUFFER.</summary>
        /// <remarks>Purpose: Makes the buffer active for indexed draw calls.
        /// Ownership: The instance retains ownership of the buffer.
        /// Thread Safety: Call only on the render thread.</remarks>
        void bind() const;

        /// <summary>Unbinds the index buffer from GL_ELEMENT_ARRAY_BUFFER.</summary>
        /// <remarks>Purpose: Clears the buffer binding.
        /// Ownership: The instance retains ownership of the buffer.
        /// Thread Safety: Call only on the render thread.</remarks>
        void unbind() const;

        /// <summary>Returns the number of indices stored in the buffer.</summary>
        /// <remarks>Purpose: Allows draw calls to know the index count.
        /// Ownership: Returns a value type; no ownership transfer.
        /// Thread Safety: Safe to call on the render thread.</remarks>
        uint32 get_count() const;

      private:
        uint32 _buffer_id = 0;
        uint32 _count = 0;
    };
}
