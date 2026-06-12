#pragma once
#include "opengl_resources/opengl_resource_cache.h"
#include "opengl_resources/opengl_state.h"
#include "tbx/systems/plugin_api/service_provider.h"
#include <chrono>
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/interfaces/opengl_context_backend.h"
#include "tbx/types/window.h"
#include <memory>

namespace opengl_rendering
{
    inline constexpr int OPENGL_MAJOR_VERSION = 4;
    inline constexpr int OPENGL_MINOR_VERSION = 6;

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

        tbx::Result begin_frame(const tbx::RenderTarget& output_target) override;
        tbx::Result end_frame() override;

        tbx::Result present() override;
        void wait_for_idle() override;

        tbx::Result read_back_buffer(
            const tbx::Size& backbuffer_size,
            std::vector<uint8>& out_pixels) override;

        tbx::Result begin_render_pass(const tbx::RenderPassDesc& pass) override;
        tbx::Result end_render_pass() override;

        tbx::Result bind_group(uint32 set_index, const tbx::GpuId& group_resource_uuid) override;
        tbx::Result bind_raster_pipeline(const tbx::GpuId& pipeline_resource_uuid) override;

        tbx::Result draw(
            uint32 index_count,
            uint32 instance_count,
            uint32 first_index,
            int32 vertex_offset,
            uint32 first_instance) override;
        tbx::Result draw_indirect(
            const tbx::GpuId& argument_buffer,
            uint64 offset,
            uint32 draw_count,
            uint32 stride) override;

        tbx::Result create_bind_group(const tbx::BindGroupDesc& desc, tbx::GpuId& out_resource_uuid)
            override;
        tbx::Result create_buffer(const tbx::BufferDesc& desc, tbx::GpuId& out_resource_uuid)
            override;
        tbx::Result create_raster_pipeline(
            const tbx::RasterPipelineDesc& desc,
            tbx::GpuId& out_resource_uuid) override;
        tbx::Result create_sampler(
            const tbx::SamplerDesc& desc,
            tbx::GpuId& out_resource_uuid) override;
        tbx::Result create_texture(
            const tbx::TextureDesc& desc,
            tbx::GpuId& out_resource_uuid) override;

        bool supports_bindless_textures() const override;
        tbx::Result get_texture_bindless_handle(
            const tbx::GpuId& texture_uuid,
            uint64& out_handle) override;

        tbx::Result write_buffer(
            const tbx::GpuId& resource_uuid,
            const void* data,
            uint64 data_size,
            uint64 offset) override;
        tbx::Result write_texture(
            const tbx::GpuId& resource_uuid,
            const tbx::TextureUpdateDesc& desc,
            const void* data,
            uint64 data_size) override;

        tbx::Result destroy_resource(const tbx::GpuId& resource_uuid) override;
        void destroy_context(const tbx::Window& window);

      private:
        tbx::Result make_current(tbx::Window window) const;
        tbx::Result present(tbx::Window window) const;
        void apply_raster_pipeline_state(const OpenGlPipelineState& state);
        void clear_bound_state();
        void cleanup();
        void destroy_resources();
        tbx::Result ensure_frame_context(const tbx::Window& window);
        tbx::Result ensure_output_framebuffer(const tbx::Size& output_size);
        void destroy_output_framebuffer();
        uint32 get_output_framebuffer() const;
        tbx::Result ensure_gl_loaded();
        tbx::GpuId next_resource_id();
        std::shared_ptr<tbx::IOpenGlContextBackend> lock_context_backend() const;
        tbx::Result require_gl_ready_for_resource_ops() const;

      private:
        std::weak_ptr<tbx::IOpenGlContextBackend> _context_backend = {};
        std::vector<tbx::Window> _contexts = {};

        OpenGlResourceCache _cache = {};
        OpenGlState _state = {};
        uint32 _readback_pbos[3] = {0U, 0U, 0U};
        void* _readback_fences[3] = {nullptr, nullptr, nullptr};
        uint32 _readback_write_index = 0U;
        uint32 _readback_inflight = 0U;
        tbx::Size _readback_pbo_size = {};
        uint32 _output_framebuffer = 0U;
        uint32 _output_color_texture = 0U;
        uint32 _output_depth_renderbuffer = 0U;
        tbx::Size _output_size = {};
        bool _is_texture_frame = false;
        std::chrono::steady_clock::time_point _last_texture_present_time = {};
        tbx::GpuId _next_resource_id = 1U;
    };
}
