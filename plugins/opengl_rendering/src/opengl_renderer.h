#pragma once
#include "tbx/graphics/render_pipeline.h"
#include "opengl_resources.h"
#include <memory>
#include <vector>

namespace opengl_rendering
{
    struct OpenGlFrameState;

    class OpenGlRenderer final : public tbx::IGraphicsBackend
    {
      public:
        OpenGlRenderer(tbx::AssetManager& asset_manager, tbx::JobSystem& job_system);
        ~OpenGlRenderer() noexcept override;

      public:
        tbx::GraphicsApi get_api() const override;
        tbx::Result initialize(tbx::GraphicsProcAddress loader) override;
        tbx::Result upload(const tbx::Mesh& mesh, tbx::Uuid& out_resource_uuid) override;
        tbx::Result upload(const tbx::Material& material, tbx::Uuid& out_resource_uuid) override;
        tbx::Result upload(const tbx::Texture& texture, tbx::Uuid& out_resource_uuid) override;
        tbx::Result upload(
            const tbx::TextureSettings& texture_settings,
            tbx::Uuid& out_resource_uuid) override;
        tbx::Result unload(const tbx::Uuid& resource_uuid) override;
        tbx::Result begin_draw(
            const tbx::Window& window,
            const tbx::Camera& view,
            const tbx::Size& resolution) override;
        tbx::RenderPassOutcome draw_shadows(const tbx::ShadowRenderInfo& shadows) override;
        tbx::RenderPassOutcome draw_geometry(const tbx::GeometryRenderInfo& geo) override;
        tbx::RenderPassOutcome draw_lighting(const tbx::LightingRenderInfo& lighting) override;
        tbx::RenderPassOutcome draw_transparent(const tbx::TransparentRenderInfo& transparency) override;
        tbx::RenderPassOutcome apply_post_processing(const tbx::PostProcessingPass& post) override;
        tbx::Result clear(const tbx::Color& color) override;
        tbx::Result end_draw() override;

      private:
        OpenGlFrameState& ensure_frame_state();
        OpenGlFrameState* try_get_frame_state();
        void build_geometry_draw_calls(
            OpenGlFrameState& state,
            const std::vector<tbx::RenderDrawItem>& draw_items);
        void build_shadow_draw_calls(
            OpenGlFrameState& state,
            const std::vector<tbx::RenderShadowItem>& draw_items);
        void build_transparent_draw_calls(
            OpenGlFrameState& state,
            const std::vector<tbx::RenderDrawItem>& draw_items);
        void initialize_runtime(tbx::GraphicsProcAddress loader);

      private:
        tbx::AssetManager& _asset_manager;
        tbx::JobSystem& _job_system;
        OpenGlResources _resources;
        std::shared_ptr<OpenGlFrameState> _state = nullptr;
        bool _is_runtime_initialized = false;
    };
}
