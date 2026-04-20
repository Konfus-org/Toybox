#pragma once
#include "OpenGlDrawCalls.h"
#include "opengl_resources/opengl_resource_manager.h"

namespace opengl_rendering
{
    class GeometryPass final
    {
      public:
        GeometryPass(const OpenGlResourceManager& resource_manager);
        GeometryPass(const GeometryPass&) = delete;
        GeometryPass& operator=(const GeometryPass&) = delete;
        ~GeometryPass() noexcept;

      public:
        void draw(
            const tbx::Color& clear_color,
            const tbx::Mat4& view_projection,
            const std::vector<DrawCall>& draw_calls);

      private:
        const OpenGlResourceManager& _resource_manager;
    };
}
