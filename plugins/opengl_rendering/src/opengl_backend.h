#pragma once
#include "opengl_resources/opengl_resource_cache.h"
#include "opengl_resources/opengl_state.h"
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/interfaces/opengl_context_backend.h"
#include "tbx/types/window.h"
#include <memory>

namespace opengl_rendering
{
    inline constexpr int OPENGL_MAJOR_VERSION = 4;
    inline constexpr int OPENGL_MINOR_VERSION = 5;

    /// @brief
    /// Purpose: Implements the Toybox explicit graphics backend using OpenGL.
    /// @details
    /// Ownership: Owns OpenGL objects and borrows the context backend service.
    /// Thread Safety: Not thread-safe; call from the thread that owns the OpenGL context.
    class OpenGlGraphicsBackend final : public tbx::IGraphicsBackend
    {
      public:
        OpenGlGraphicsBackend(std::weak_ptr<tbx::IOpenGlContextBackend> context_backend);
        ~OpenGlGraphicsBackend() noexcept override;

      public:
        OpenGlGraphicsBackend(const OpenGlGraphicsBackend&) = delete;
        OpenGlGraphicsBackend& operator=(const OpenGlGraphicsBackend&) = delete;

      public:
        tbx::GraphicsApi get_api() const override;
        tbx::VsyncMode get_vsync() const override;
        tbx::Result set_vsync(tbx::VsyncMode mode) override;

        tbx::Result begin_frame(const tbx::Window& output_target) override;
        tbx::Result end_frame() override;

        tbx::Result present() override;
        void wait_for_idle() override;

        tbx::Result begin_render_pass(const tbx::GraphicsRenderPassDesc& pass) override;
        tbx::Result end_render_pass() override;
        tbx::Result begin_compute_pass(const tbx::GraphicsComputePassDesc& pass) override;
        tbx::Result end_compute_pass() override;

        tbx::Result bind_group(uint32 set_index, const tbx::Uuid& group_resource_uuid) override;
        tbx::Result bind_compute_pipeline(const tbx::Uuid& pipeline_resource_uuid) override;
        tbx::Result bind_raster_pipeline(const tbx::Uuid& pipeline_resource_uuid) override;

        tbx::Result pipeline_barrier(
            const std::vector<tbx::PipelineBarrierDesc>& barriers) override;

        tbx::Result draw(
            uint32 index_count,
            uint32 instance_count,
            uint32 first_index,
            int32 vertex_offset,
            uint32 first_instance) override;
        tbx::Result draw_indirect(
            const tbx::Uuid& argument_buffer,
            uint64 offset,
            uint32 draw_count,
            uint32 stride) override;
        tbx::Result dispatch_compute(
            uint32 group_count_x,
            uint32 group_count_y,
            uint32 group_count_z) override;

        tbx::Result create_bind_group(const tbx::BindGroupDesc& desc, tbx::Uuid& out_resource_uuid)
            override;
        tbx::Result create_bind_group_layout(
            const tbx::BindGroupLayoutDesc& desc,
            tbx::Uuid& out_resource_uuid) override;
        tbx::Result create_buffer(const tbx::GraphicsBufferDesc& desc, tbx::Uuid& out_resource_uuid)
            override;
        tbx::Result create_compute_pipeline(
            const tbx::ComputePipelineDesc& desc,
            tbx::Uuid& out_resource_uuid) override;
        tbx::Result create_raster_pipeline(
            const tbx::RasterPipelineDesc& desc,
            tbx::Uuid& out_resource_uuid) override;
        tbx::Result create_sampler(
            const tbx::GraphicsSamplerDesc& desc,
            tbx::Uuid& out_resource_uuid) override;
        tbx::Result create_texture(
            const tbx::GraphicsTextureDesc& desc,
            tbx::Uuid& out_resource_uuid) override;

        tbx::Result write_buffer(
            const tbx::Uuid& resource_uuid,
            const void* data,
            uint64 data_size,
            uint64 offset) override;
        tbx::Result write_texture(
            const tbx::Uuid& resource_uuid,
            const tbx::GraphicsTextureUpdateDesc& desc,
            const void* data,
            uint64 data_size) override;

        tbx::Result destroy_resource(const tbx::Uuid& resource_uuid) override;
        void destroy_context(const tbx::Window& window);

      private:
        tbx::Result make_current(tbx::Window window) const;
        tbx::Result present(tbx::Window window) const;
        void apply_raster_pipeline_state(const OpenGlPipelineState& state);
        void clear_bound_state();
        void cleanup();
        void destroy_resources();
        tbx::Result ensure_frame_context(const tbx::Window& window);
        tbx::Result ensure_gl_loaded();
        std::shared_ptr<tbx::IOpenGlContextBackend> lock_context_backend() const;
        tbx::Result require_gl_ready_for_resource_ops() const;

      private:
        std::weak_ptr<tbx::IOpenGlContextBackend> _context_backend = {};
        std::vector<tbx::Window> _contexts = {};

        OpenGlResourceCache _cache = {};
        OpenGlState _state = {};
    };
}
