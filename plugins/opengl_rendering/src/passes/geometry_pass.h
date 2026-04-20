#pragma once
#include "open_gl_draw_calls.h"
#include "opengl_resources.h"

namespace opengl_rendering
{
    class GeometryPass final
    {
      public:
        GeometryPass(const OpenGlResources& resources);
        GeometryPass(const GeometryPass&) = delete;
        GeometryPass& operator=(const GeometryPass&) = delete;
        ~GeometryPass() noexcept;

      public:
        void draw(
            const tbx::Color& clear_color,
            const tbx::Mat4& view_projection,
            const std::vector<DrawCall>& draw_calls);

      private:
        const OpenGlResources& _resources;
    };
}
