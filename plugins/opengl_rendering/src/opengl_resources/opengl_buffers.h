#pragma once
#include "opengl_resource.h"
#include "opengl_texture.h"
#include "tbx/common/int.h"
#include "tbx/graphics/mesh.h"
#include "tbx/graphics/texture.h"
#include "tbx/graphics/vertex.h"
#include "tbx/math/size.h"
#include <cstddef>
#include <memory>

namespace tbx::plugins
{
    /// <summary>Defines how a framebuffer is mapped into a presentation destination.</summary>
    /// <remarks>Purpose: Controls scaling behavior during framebuffer blit presentation.
    /// Ownership: Value semantics only; no ownership transfer.
    /// Thread Safety: Immutable enum safe to read across threads.</remarks>
    enum class OpenGlFrameBufferPresentMode
    {
        ASPECT_FIT,
        STRETCH
    };

    /// <summary>Owns an OpenGL framebuffer with color and depth-stencil attachments.</summary>
    /// <remarks>Purpose: Provides a resizable render target for multi-pass rendering.
    /// Ownership: Owns framebuffer, color texture, and depth-stencil renderbuffer handles.
    /// Thread Safety: Not thread-safe; use on the render thread.</remarks>
    class OpenGlFrameBuffer final : public IOpenGlResource
    {
      public:
        /// <summary>Creates an empty framebuffer object.</summary>
        /// <remarks>Purpose: Defers attachment allocation until set_resolution() is called.
        /// Ownership: Owns no GPU handles until initialized.
        /// Thread Safety: Construct on the render thread.</remarks>
        OpenGlFrameBuffer() = default;

        /// <summary>Creates and allocates a framebuffer for the provided size.</summary>
        /// <remarks>Purpose: Initializes framebuffer attachments immediately.
        /// Ownership: Owns created GPU handles.
        /// Thread Safety: Construct on the render thread.</remarks>
        explicit OpenGlFrameBuffer(const Size& resolution);

        /// <summary>Creates and allocates a framebuffer with explicit filtering.</summary>
        /// <remarks>Purpose: Initializes framebuffer attachments and color sampling mode.
        /// Ownership: Owns created GPU handles.
        /// Thread Safety: Construct on the render thread.</remarks>
        OpenGlFrameBuffer(const Size& resolution, TextureFilter filtering);

        OpenGlFrameBuffer(const OpenGlFrameBuffer&) = delete;
        OpenGlFrameBuffer& operator=(const OpenGlFrameBuffer&) = delete;

        /// <summary>Destroys the framebuffer and all attachments.</summary>
        /// <remarks>Purpose: Releases owned GPU handles.
        /// Ownership: Destroys GPU resources owned by this instance.
        /// Thread Safety: Destroy on the render thread.</remarks>
        ~OpenGlFrameBuffer() noexcept override;

        /// <summary>Binds this framebuffer for rendering.</summary>
        /// <remarks>Purpose: Sets GL_FRAMEBUFFER binding to this instance.
        /// Ownership: Does not transfer ownership.
        /// Thread Safety: Call on the render thread.</remarks>
        void bind() override;

        /// <summary>Unbinds this framebuffer from rendering.</summary>
        /// <remarks>Purpose: Restores GL_FRAMEBUFFER binding to default framebuffer.
        /// Ownership: Does not transfer ownership.
        /// Thread Safety: Call on the render thread.</remarks>
        void unbind() override;

        /// <summary>Returns the allocated framebuffer resolution.</summary>
        /// <remarks>Purpose: Exposes render target dimensions used for rasterization.
        /// Ownership: Returns value; no ownership transfer.
        /// Thread Safety: Safe on render thread.</remarks>
        Size get_resolution() const;

        /// <summary>Returns the OpenGL framebuffer handle.</summary>
        /// <remarks>Purpose: Allows use in low-level OpenGL operations.
        /// Ownership: Returns value; no ownership transfer.
        /// Thread Safety: Safe on render thread.</remarks>
        uint32 get_framebuffer_id() const;

        /// <summary>Returns the OpenGL color attachment texture handle.</summary>
        /// <remarks>Purpose: Allows sampling/blitting from the color buffer.
        /// Ownership: Returns value; no ownership transfer.
        /// Thread Safety: Safe on render thread.</remarks>
        uint32 get_color_texture_id() const;

        /// <summary>Returns shared ownership of the color attachment texture object.</summary>
        /// <remarks>Purpose: Exposes the runtime OpenGL texture wrapper for registration.
        /// Ownership: Returns shared ownership.
        /// Thread Safety: Not thread-safe; use on render thread.</remarks>
        std::shared_ptr<OpenGlTexture> get_color_texture() const;

        /// <summary>Updates the color attachment filtering mode.</summary>
        /// <remarks>Purpose: Controls min/mag sampling behavior used when presenting this
        /// framebuffer.
        /// Ownership: Stores value in this framebuffer; no ownership transfer.
        /// Thread Safety: Call on the render thread.</remarks>
        void set_filtering(TextureFilter filtering);

        /// <summary>Returns the active color attachment filtering mode.</summary>
        /// <remarks>Purpose: Exposes how the framebuffer color texture is sampled during blit.
        /// Ownership: Returns value; no ownership transfer.
        /// Thread Safety: Safe on render thread.</remarks>
        TextureFilter get_filtering() const;

        /// <summary>Resizes the framebuffer attachments.</summary>
        /// <remarks>Purpose: Recreates attachments to match the requested render resolution.
        /// Ownership: Replaces and owns newly created GPU handles.
        /// Thread Safety: Call on the render thread.</remarks>
        void set_resolution(const Size& resolution);

        /// <summary>Resizes the framebuffer attachments.</summary>
        /// <remarks>Purpose: Backward-compatible alias for set_resolution().
        /// Ownership: Replaces and owns newly created GPU handles.
        /// Thread Safety: Call on the render thread.</remarks>
        void set_size(const Size& size);

        /// <summary>Presents this framebuffer into the requested destination framebuffer.</summary>
        /// <remarks>Purpose: Blits the framebuffer color attachment into a destination target with
        /// configurable scaling mode.
        /// Ownership: Does not transfer ownership of source or destination framebuffers.
        /// Thread Safety: Call on the render thread with a current OpenGL context.</remarks>
        void preset(
            uint32 destination_framebuffer_id,
            const Size& destination_size,
            OpenGlFrameBufferPresentMode mode) const;

      private:
        uint32 _framebuffer_id = 0;
        std::shared_ptr<OpenGlTexture> _color_texture = nullptr;
        uint32 _depth_stencil_renderbuffer_id = 0;
        Size _resolution = {};
        TextureFilter _filtering = TextureFilter::LINEAR;
    };

    /// <summary>Owns an OpenGL vertex buffer object.</summary>
    /// <remarks>Purpose: Uploads vertex data and configures attribute pointers.
    /// Ownership: Owns the OpenGL buffer identifier and releases it on destruction.
    /// Thread Safety: Not thread-safe; use on the render thread.</remarks>
    class OpenGlVertexBuffer final : public IOpenGlResource
    {
      public:
        OpenGlVertexBuffer();
        ~OpenGlVertexBuffer() noexcept;

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
        void bind() override;

        /// <summary>Unbinds the vertex buffer from GL_ARRAY_BUFFER.</summary>
        /// <remarks>Purpose: Clears the buffer binding.
        /// Ownership: The instance retains ownership of the buffer.
        /// Thread Safety: Call only on the render thread.</remarks>
        void unbind() override;

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
    class OpenGlIndexBuffer final : public IOpenGlResource
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
        ~OpenGlIndexBuffer() noexcept;

        /// <summary>Uploads index data to the buffer.</summary>
        /// <remarks>Purpose: Copies index data into GPU memory.
        /// Ownership: Copies from the provided buffer; caller retains CPU ownership.
        /// Thread Safety: Call only on the render thread.</remarks>
        void upload(const IndexBuffer& buffer);

        /// <summary>Binds the index buffer to GL_ELEMENT_ARRAY_BUFFER.</summary>
        /// <remarks>Purpose: Makes the buffer active for indexed draw calls.
        /// Ownership: The instance retains ownership of the buffer.
        /// Thread Safety: Call only on the render thread.</remarks>
        void bind() override;

        /// <summary>Unbinds the index buffer from GL_ELEMENT_ARRAY_BUFFER.</summary>
        /// <remarks>Purpose: Clears the buffer binding.
        /// Ownership: The instance retains ownership of the buffer.
        /// Thread Safety: Call only on the render thread.</remarks>
        void unbind() override;

        /// <summary>Returns the number of indices stored in the buffer.</summary>
        /// <remarks>Purpose: Allows draw calls to know the index count.
        /// Ownership: Returns a value type; no ownership transfer.
        /// Thread Safety: Safe to call on the render thread.</remarks>
        uint32 get_count() const;

      private:
        uint32 _buffer_id = 0;
        uint32 _count = 0;
    };

    /// <summary>Owns an OpenGL shader-storage buffer object.</summary>
    /// <remarks>Purpose: Uploads and binds structured GPU data for compute and shading passes.
    /// Ownership: Owns the OpenGL buffer identifier and allocated byte storage.
    /// Thread Safety: Not thread-safe; use on the render thread.</remarks>
    class OpenGlStorageBuffer final : public IOpenGlResource
    {
      public:
        /// <summary>Creates an empty storage buffer object.</summary>
        /// <remarks>Purpose: Allocates an OpenGL buffer handle for SSBO usage.
        /// Ownership: Owns the created GPU handle.
        /// Thread Safety: Construct on the render thread.</remarks>
        OpenGlStorageBuffer();

        /// <summary>Destroys the storage buffer object.</summary>
        /// <remarks>Purpose: Releases the OpenGL buffer handle.
        /// Ownership: Owns the GPU handle being destroyed.
        /// Thread Safety: Destroy on the render thread.</remarks>
        ~OpenGlStorageBuffer() noexcept override;

        /// <summary>Ensures the buffer capacity is at least the requested byte count.</summary>
        /// <remarks>Purpose: Resizes GPU storage when capacity is insufficient.
        /// Ownership: Reallocates and owns GPU storage.
        /// Thread Safety: Call only on the render thread.</remarks>
        void ensure_capacity(size_t byte_count);

        /// <summary>Uploads bytes into the buffer at an offset.</summary>
        /// <remarks>Purpose: Copies CPU data into GPU storage.
        /// Ownership: Copies data; caller retains CPU ownership.
        /// Thread Safety: Call only on the render thread.</remarks>
        void upload(const void* data, size_t byte_count, size_t byte_offset = 0U);

        /// <summary>Clears the buffer with an unsigned integer value pattern.</summary>
        /// <remarks>Purpose: Initializes/reset buffer contents quickly on GPU.
        /// Ownership: Does not transfer ownership.
        /// Thread Safety: Call only on the render thread.</remarks>
        void clear_u32(uint32 value) const;

        /// <summary>Binds this buffer to a shader-storage binding point.</summary>
        /// <remarks>Purpose: Exposes the buffer to shader programs via SSBO binding.
        /// Ownership: Does not transfer ownership.
        /// Thread Safety: Call only on the render thread.</remarks>
        void bind_base(uint32 binding_point) const;

        /// <summary>Returns the OpenGL buffer handle.</summary>
        /// <remarks>Purpose: Supports low-level OpenGL calls and debug instrumentation.
        /// Ownership: Returns a value; no ownership transfer.
        /// Thread Safety: Safe on the render thread.</remarks>
        uint32 get_buffer_id() const;

        /// <summary>Returns current allocated capacity in bytes.</summary>
        /// <remarks>Purpose: Allows callers to size uploads and dispatches.
        /// Ownership: Returns a value; no ownership transfer.
        /// Thread Safety: Safe on the render thread.</remarks>
        size_t get_capacity_bytes() const;

        /// <summary>Binds this buffer to GL_SHADER_STORAGE_BUFFER.</summary>
        /// <remarks>Purpose: Makes the buffer active for non-indexed SSBO operations.
        /// Ownership: Does not transfer ownership.
        /// Thread Safety: Call only on the render thread.</remarks>
        void bind() override;

        /// <summary>Unbinds GL_SHADER_STORAGE_BUFFER.</summary>
        /// <remarks>Purpose: Clears the non-indexed SSBO binding.
        /// Ownership: Does not transfer ownership.
        /// Thread Safety: Call only on the render thread.</remarks>
        void unbind() override;

      private:
        uint32 _buffer_id = 0U;
        size_t _capacity_bytes = 0U;
    };
}
