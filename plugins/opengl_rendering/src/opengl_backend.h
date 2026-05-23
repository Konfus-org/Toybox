#pragma once
#include "opengl_context.h"
#include "opengl_resources/opengl_buffers.h"
#include "opengl_resources/opengl_sampler.h"
#include "opengl_resources/opengl_shader.h"
#include "opengl_resources/opengl_texture.h"
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/interfaces/opengl_context_manager.h"
#include <glad/glad.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace opengl_rendering
{
    /// @brief
    /// Purpose: Implements the Toybox explicit graphics backend using OpenGL.
    /// @details
    /// Ownership: Owns OpenGL objects and borrows the context manager service.
    /// Thread Safety: Not thread-safe; call from the thread that owns the OpenGL context.
    class OpenGlGraphicsBackend final : public tbx::IGraphicsBackend
    {
      public:
        OpenGlGraphicsBackend(tbx::IOpenGlContextManager& context_manager);
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

        tbx::Result destroy_resource(const tbx::Uuid& resource_uuid) override;
        tbx::Result pipeline_barrier(
            const std::vector<tbx::PipelineBarrierDesc>& barriers) override;
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

        void destroy_context(const tbx::Window& window);

      private:
        tbx::Result bind_index_buffer(
            const tbx::Uuid& buffer_resource_uuid,
            tbx::GraphicsIndexType index_type);
        tbx::Result bind_sampler(uint32 slot, const tbx::Uuid& sampler_resource_uuid);
        tbx::Result bind_storage_buffer(uint32 slot, const tbx::Uuid& buffer_resource_uuid);
        tbx::Result bind_texture(uint32 slot, const tbx::Uuid& texture_resource_uuid);
        tbx::Result bind_uniform_buffer(uint32 slot, const tbx::Uuid& buffer_resource_uuid);
        tbx::Result bind_vertex_buffer(uint32 slot, const tbx::Uuid& buffer_resource_uuid);
        tbx::Result set_viewport(const tbx::Viewport& viewport);
        void clear_bound_state();
        void cleanup();
        void destroy_resources();
        tbx::Result ensure_frame_context(const tbx::Window& window);
        tbx::Result ensure_gl_loaded();
        tbx::Result require_gl_ready_for_resource_ops() const;
        tbx::Result require_current_compute_pipeline() const;
        tbx::Result require_current_raster_pipeline() const;
        tbx::Result bind_storage_texture(uint32 slot, const tbx::Uuid& texture_resource_uuid);

      private:
        tbx::IOpenGlContextManager& _context_manager;

        std::unordered_map<tbx::Uuid, tbx::BindGroupDesc> _bind_groups = {};
        std::unordered_map<tbx::Uuid, tbx::BindGroupLayoutDesc> _bind_group_layouts = {};
        std::unordered_map<tbx::Uuid, OpenGlGraphicsBuffer> _buffers = {};
        std::unordered_map<tbx::Uuid, tbx::GraphicsBufferDesc> _buffer_descs = {};
        std::unordered_map<tbx::Uuid, tbx::ComputePipelineDesc> _compute_pipeline_descs = {};
        std::unordered_map<tbx::Uuid, OpenGlShaderProgram> _programs = {};
        std::unordered_map<tbx::Uuid, tbx::RasterPipelineDesc> _raster_pipeline_descs = {};
        std::unordered_map<tbx::Uuid, GLuint> _pipeline_vertex_arrays = {};
        std::unordered_map<tbx::Uuid, OpenGlSampler> _samplers = {};
        std::unordered_map<tbx::Uuid, OpenGlTexture> _textures = {};
        std::unordered_map<tbx::Uuid, tbx::GraphicsTextureDesc> _texture_descs = {};
        std::unordered_map<tbx::Window, OpenGlContext> _contexts = {};
        std::unique_ptr<OpenGlFramebuffer> _pass_framebuffer = {};

        tbx::Window _active_window = {};
        tbx::Viewport _active_viewport = {};
        tbx::Uuid _current_pipeline = {};
        tbx::GraphicsIndexType _current_index_type = tbx::GraphicsIndexType::UINT32;
        tbx::VsyncMode _vsync_mode = tbx::VsyncMode::OFF;

        bool _is_gl_loaded = false;
        bool _is_compute_pass_active = false;
        bool _is_pass_active = false;
        bool _is_index_buffer_bound = false;
    };
}
