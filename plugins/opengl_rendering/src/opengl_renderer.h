#pragma once
#include "tbx/graphics/render_pipeline.h"
#include "opengl_uploader.h"
#include <memory>
#include <vector>

namespace opengl_rendering
{
    struct OpenGlWindowRendererState;

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
            const tbx::Size& resolution,
            const tbx::Color& clear_color) override;
        tbx::Result draw_shadows(const tbx::ShadowRenderInfo& shadows) override;
        tbx::Result draw_geometry(const tbx::GeometryRenderInfo& geo) override;
        tbx::Result draw_lighting(const tbx::LightingRenderInfo& lighting) override;
        tbx::Result draw_transparent(const tbx::TransparentRenderInfo& transparency) override;
        tbx::Result apply_post_processing(const tbx::PostProcessingPass& post) override;
        tbx::Result end_draw() override;

      private:
        OpenGlWindowRendererState& ensure_state();
        OpenGlWindowRendererState* try_get_active_state();
        void build_geometry_draw_calls(
            OpenGlWindowRendererState& state,
            const std::vector<tbx::RenderDrawItem>& draw_items);
        void build_shadow_draw_calls(
            OpenGlWindowRendererState& state,
            const std::vector<tbx::RenderShadowItem>& draw_items);
        void build_transparent_draw_calls(
            OpenGlWindowRendererState& state,
            const std::vector<tbx::RenderDrawItem>& draw_items);
        void initialize_runtime(tbx::GraphicsProcAddress loader);

      private:
        tbx::AssetManager& _asset_manager;
        tbx::JobSystem& _job_system;
        OpenGlUploader _resource_manager;
        std::shared_ptr<OpenGlWindowRendererState> _state = nullptr;
        bool _is_runtime_initialized = false;
    };
}
