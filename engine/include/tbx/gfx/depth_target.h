#pragma once
#include "tbx/core/typedefs.h"

namespace tbx::gpu
{
    /// @brief
    /// Purpose: Depth-only render target for shadow passes — RAII via the backend.
    /// Obtain via make_depth_target(); render into it via a depth-only render pass.
    class DepthTarget final
    {
      public:
        DepthTarget(uint32 framebuffer, uint32 depth_texture, int resolution)
            : _framebuffer(framebuffer)
            , _depth_texture(depth_texture)
            , _resolution(resolution)
        {
        }
        ~DepthTarget();

      public:
        DepthTarget(const DepthTarget&) = delete;
        DepthTarget& operator=(const DepthTarget&) = delete;

      public:
        /// @brief
        /// Purpose: Backend-native depth texture id.
        uint32 get_depth_texture() const
        {
            return _depth_texture;
        }

        /// @brief
        /// Purpose: Backend-native framebuffer id.
        uint32 get_framebuffer() const
        {
            return _framebuffer;
        }

        /// @brief
        /// Purpose: Square resolution in pixels.
        int get_resolution() const
        {
            return _resolution;
        }

      private:
        uint32 _framebuffer = 0;
        uint32 _depth_texture = 0;
        int _resolution = 0;
    };
}
