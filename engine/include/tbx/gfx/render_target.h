#pragma once
#include "tbx/utils/api.h"
#include "tbx/utils/typedefs.h"

namespace tbx::gpu
{
    /// @brief
    /// Purpose: Offscreen color+depth render target (post-processing, editor viewports) —
    /// RAII via the backend. Obtain via make_render_target(); render into it via a render
    /// pass whose color attachment names it.
    class TBX_API RenderTarget final
    {
      public:
        RenderTarget(
            uint32 framebuffer,
            uint32 color_texture,
            uint32 depth_buffer,
            int width,
            int height)
            : _framebuffer(framebuffer)
            , _color_texture(color_texture)
            , _depth_buffer(depth_buffer)
            , _width(width)
            , _height(height)
        {
        }
        ~RenderTarget();

      public:
        RenderTarget(const RenderTarget&) = delete;
        RenderTarget& operator=(const RenderTarget&) = delete;

      public:
        /// @brief
        /// Purpose: Backend-native color texture id.
        uint32 get_color_texture() const
        {
            return _color_texture;
        }

        /// @brief
        /// Purpose: Backend-native depth buffer id.
        uint32 get_depth_buffer() const
        {
            return _depth_buffer;
        }

        /// @brief
        /// Purpose: Backend-native framebuffer id.
        uint32 get_framebuffer() const
        {
            return _framebuffer;
        }

        /// @brief
        /// Purpose: Height in pixels.
        int get_height() const
        {
            return _height;
        }

        /// @brief
        /// Purpose: Width in pixels.
        int get_width() const
        {
            return _width;
        }

      private:
        uint32 _framebuffer = 0;
        uint32 _color_texture = 0;
        uint32 _depth_buffer = 0;
        int _width = 0;
        int _height = 0;
    };
}
