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
    struct OpenGlRenderer final
    {
        OpenGlRenderer(
            tbx::GraphicsProcAddress loader,
            tbx::EntityRegistry& entity_registry,
            tbx::AssetManager& asset_manager,
            OpenGlContext context);
        ~OpenGlRenderer() noexcept;

        bool render();

        void set_viewport_size(const tbx::Size& viewport_size);
        void set_pending_render_resolution(
            const std::optional<tbx::Size>& pending_render_resolution);

        const OpenGlContext& get_context() const;

      private:
        void initialize(tbx::GraphicsProcAddress loader) const;
        void shutdown();
        void set_render_resolution(const tbx::Size& render_resolution);

      private:
        OpenGlContext _context;
        tbx::EntityRegistry& _entity_registry;
        OpenGlResourceManager _resource_manager;
        std::unique_ptr<OpenGlRenderPipeline> _render_pipeline = nullptr;

        tbx::Size _viewport_size = {0, 0};
        tbx::Size _render_resolution = {0, 0};
        std::optional<tbx::Size> _pending_render_resolution = std::nullopt;
    };
}
