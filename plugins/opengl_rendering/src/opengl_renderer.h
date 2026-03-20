#pragma once
#include "opengl_context.h"
#include "opengl_resources/opengl_resource_manager.h"
#include "opengl_resources/opengl_texture.h"
#include "pipeline/OpenGlFrameContext.h"
#include "pipeline/OpenGlGBuffer.h"
#include "pipeline/opengl_render_pipeline.h"
#include "tbx/assets/asset_manager.h"
#include "tbx/async/job_system.h"
#include "tbx/ecs/entity_registry.h"
#include "tbx/graphics/messages.h"
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>

namespace opengl_rendering
{
    struct OpenGlRenderer final
    {
        OpenGlRenderer(
            tbx::GraphicsProcAddress loader,
            tbx::EntityRegistry& entity_registry,
            tbx::AssetManager& asset_manager,
            tbx::JobSystem& job_system,
            OpenGlContext context);
        ~OpenGlRenderer() noexcept;

        bool render();

        void set_viewport_size(const tbx::Size& viewport_size);
        void set_pending_render_resolution(
            const std::optional<tbx::Size>& pending_render_resolution);
        const OpenGlContext& get_context() const;
        void set_render_stage(tbx::RenderStage render_stage);

      private:
        void initialize(tbx::GraphicsProcAddress loader) const;
        void shutdown();
        void set_render_resolution(const tbx::Size& render_resolution);
        OpenGlFrameContext build_frame_context() const;
        void build_light_data(OpenGlFrameContext& frame_context) const;
        void build_shadow_data(OpenGlFrameContext& frame_context) const;
        void build_draw_calls(OpenGlFrameContext& frame_context);

      private:
        OpenGlContext _context;
        tbx::EntityRegistry& _entity_registry;
        tbx::AssetManager& _asset_manager;
        tbx::JobSystem& _job_system;
        OpenGlResourceManager _resource_manager;
        std::unique_ptr<OpenGlRenderPipeline> _render_pipeline = nullptr;
        std::unordered_map<tbx::Uuid, tbx::Material> _material_defaults_cache = {};

        tbx::Size _viewport_size = {0, 0};
        tbx::Size _render_resolution = {0, 0};
        std::optional<tbx::Size> _pending_render_resolution = std::nullopt;
        tbx::RenderStage _render_stage = tbx::RenderStage::FINAL_COLOR;
        OpenGlGBuffer _gbuffer = {};
    };
}
