#pragma once
#include "opengl_context.h"
#include "opengl_resources/opengl_buffers.h"
#include "opengl_resources/opengl_resource_manager.h"
#include "pipeline/opengl_render_pipeline.h"
#include "tbx/assets/asset_manager.h"
#include "tbx/common/int.h"
#include "tbx/common/uuid.h"
#include "tbx/ecs/entity_registry.h"
#include "tbx/graphics/messages.h"
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace opengl_rendering
{
    using namespace tbx;
    struct OpenGlRenderer final
    {
        OpenGlRenderer(
            GraphicsProcAddress loader,
            EntityRegistry& entity_registry,
            AssetManager& asset_manager,
            OpenGlContext context);
        ~OpenGlRenderer() noexcept;

        bool render() const;

        void set_viewport_size(const Size& viewport_size);
        void set_pending_render_resolution(const std::optional<Size>& pending_render_resolution);

        const OpenGlContext& get_context() const;

      private:
        void initialize(GraphicsProcAddress loader) const;
        void shutdown();
        void set_render_resolution(const Size& render_resolution);

      private:
        std::unique_ptr<OpenGlRenderPipeline> _render_pipeline = nullptr;
        std::reference_wrapper<EntityRegistry> _entity_registry;
        OpenGlContext _context;

        Size _viewport_size = {0, 0};
        Size _render_resolution = {0, 0};
        std::optional<Size> _pending_render_resolution = std::nullopt;
    };
}
