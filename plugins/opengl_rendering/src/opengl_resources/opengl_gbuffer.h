#pragma once
#include "opengl_resource.h"
#include "tbx/common/int.h"
#include "tbx/graphics/color.h"
#include "tbx/math/size.h"

namespace tbx::plugins
{
    /// <summary>
    /// Purpose: Owns the deferred geometry buffer with MRT color attachments and a depth texture.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns framebuffer and attachment texture handles.
    /// Thread Safety: Not thread-safe; use on the render thread.
    /// </remarks>
    class OpenGlGBuffer final : public IOpenGlResource
    {
      public:
        /// <summary>
        /// Purpose: Creates an empty G-buffer.
        /// </summary>
        /// <remarks>
        /// Ownership: Owns no GPU handles until set_resolution() is called.
        /// Thread Safety: Construct on the render thread.
        /// </remarks>
        OpenGlGBuffer() = default;

        /// <summary>
        /// Purpose: Creates and allocates a G-buffer for the requested resolution.
        /// </summary>
        /// <remarks>
        /// Ownership: Owns created GPU handles.
        /// Thread Safety: Construct on the render thread.
        /// </remarks>
        explicit OpenGlGBuffer(const Size& resolution);

        OpenGlGBuffer(const OpenGlGBuffer&) = delete;
        OpenGlGBuffer& operator=(const OpenGlGBuffer&) = delete;

        /// <summary>
        /// Purpose: Destroys the G-buffer and releases all owned attachments.
        /// </summary>
        /// <remarks>
        /// Ownership: Releases GPU handles owned by this instance.
        /// Thread Safety: Destroy on the render thread.
        /// </remarks>
        ~OpenGlGBuffer() noexcept override;

        /// <summary>
        /// Purpose: Binds this G-buffer for rendering.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not transfer ownership.
        /// Thread Safety: Call on the render thread.
        /// </remarks>
        void bind() override;

        /// <summary>
        /// Purpose: Unbinds this G-buffer.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not transfer ownership.
        /// Thread Safety: Call on the render thread.
        /// </remarks>
        void unbind() override;

        /// <summary>
        /// Purpose: Recreates attachments for a new resolution.
        /// </summary>
        /// <remarks>
        /// Ownership: Replaces and owns newly created GPU handles.
        /// Thread Safety: Call on the render thread.
        /// </remarks>
        void set_resolution(const Size& resolution);

        /// <summary>
        /// Purpose: Clears G-buffer attachments to defaults for a new frame.
        /// </summary>
        /// <remarks>
        /// Ownership: Writes to this framebuffer's attachments only.
        /// Thread Safety: Call on the render thread.
        /// </remarks>
        void clear(const RgbaColor& clear_color) const;

        /// <summary>
        /// Purpose: Returns the framebuffer resolution.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns value; no ownership transfer.
        /// Thread Safety: Safe on the render thread.
        /// </remarks>
        Size get_resolution() const;

        /// <summary>
        /// Purpose: Returns the G-buffer framebuffer identifier.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns value; no ownership transfer.
        /// Thread Safety: Safe on the render thread.
        /// </remarks>
        uint32 get_framebuffer_id() const;

        /// <summary>
        /// Purpose: Returns the albedo/specular attachment texture identifier.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns value; no ownership transfer.
        /// Thread Safety: Safe on the render thread.
        /// </remarks>
        uint32 get_albedo_spec_texture_id() const;

        /// <summary>
        /// Purpose: Returns the normal attachment texture identifier.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns value; no ownership transfer.
        /// Thread Safety: Safe on the render thread.
        /// </remarks>
        uint32 get_normal_texture_id() const;

        /// <summary>
        /// Purpose: Returns the material attachment texture identifier.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns value; no ownership transfer.
        /// Thread Safety: Safe on the render thread.
        /// </remarks>
        uint32 get_material_texture_id() const;

        /// <summary>
        /// Purpose: Returns the depth attachment texture identifier.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns value; no ownership transfer.
        /// Thread Safety: Safe on the render thread.
        /// </remarks>
        uint32 get_depth_texture_id() const;

      private:
        uint32 _framebuffer_id = 0;
        uint32 _albedo_spec_texture_id = 0;
        uint32 _normal_texture_id = 0;
        uint32 _material_texture_id = 0;
        uint32 _depth_texture_id = 0;
        Size _resolution = {};
    };
}
