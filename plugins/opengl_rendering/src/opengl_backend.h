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
#include <unordered_map>

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
        tbx::Result initialize(const tbx::GraphicsSettings& settings) override;
        void shutdown() override;

        tbx::GraphicsApi get_api() const override;

        tbx::Result begin_frame(const tbx::GraphicsFrameInfo& frame) override;
        tbx::Result begin_view(const tbx::GraphicsView& view) override;
        tbx::Result end_frame() override;
        tbx::Result end_view() override;

        tbx::Result present() override;
        void wait_for_idle() override;

        tbx::Result begin_pass(const tbx::GraphicsPassDesc& pass) override;
        tbx::Result end_pass() override;

        tbx::Result set_viewport(const tbx::Viewport& viewport) override;
        tbx::Result set_scissor(const tbx::Viewport& scissor) override;

        tbx::Result bind_pipeline(const tbx::Uuid& pipeline_resource_uuid) override;
        tbx::Result bind_vertex_buffer(uint32 slot, const tbx::Uuid& buffer_resource_uuid) override;
        tbx::Result bind_index_buffer(
            const tbx::Uuid& buffer_resource_uuid,
            tbx::GraphicsIndexType index_type) override;
        tbx::Result bind_uniform_buffer(uint32 slot, const tbx::Uuid& buffer_resource_uuid)
            override;
        tbx::Result bind_storage_buffer(uint32 slot, const tbx::Uuid& buffer_resource_uuid)
            override;
        tbx::Result bind_texture(uint32 slot, const tbx::Uuid& texture_resource_uuid) override;
        tbx::Result bind_sampler(uint32 slot, const tbx::Uuid& sampler_resource_uuid) override;

        tbx::Result draw(uint32 vertex_count, uint32 vertex_offset) override;
        tbx::Result draw_indexed(const tbx::GraphicsDrawIndexedDesc& draw) override;

        tbx::Result unload(const tbx::Uuid& resource_uuid) override;
        tbx::Result upload_buffer(
            const tbx::GraphicsBufferDesc& desc,
            const void* data,
            uint64 data_size,
            tbx::Uuid& out_resource_uuid) override;
        tbx::Result upload_pipeline(
            const tbx::GraphicsPipelineDesc& desc,
            tbx::Uuid& out_resource_uuid) override;
        tbx::Result upload_sampler(
            const tbx::GraphicsSamplerDesc& desc,
            tbx::Uuid& out_resource_uuid) override;
        tbx::Result upload_texture(
            const tbx::GraphicsTextureDesc& desc,
            const void* data,
            uint64 data_size,
            tbx::Uuid& out_resource_uuid) override;
        tbx::Result update_settings(const tbx::GraphicsSettings& settings) override;
        tbx::Result update_buffer(
            const tbx::Uuid& resource_uuid,
            const void* data,
            uint64 data_size,
            uint64 offset) override;
        tbx::Result update_texture(
            const tbx::Uuid& resource_uuid,
            const tbx::GraphicsTextureUpdateDesc& desc,
            const void* data,
            uint64 data_size) override;
        void destroy_context(const tbx::Window& window);

      private:
        void clear_bound_state();
        void destroy_resources();
        tbx::Result ensure_frame_context(const tbx::Window& window);
        tbx::Result ensure_gl_loaded();
        tbx::Result require_current_pipeline() const;

      private:
        tbx::IOpenGlContextManager& _context_manager;
        std::unordered_map<tbx::Uuid, OpenGlGraphicsBuffer> _buffers = {};
        std::unordered_map<tbx::Uuid, tbx::GraphicsBufferDesc> _buffer_descs = {};
        std::unordered_map<tbx::Uuid, OpenGlShaderProgram> _programs = {};
        std::unordered_map<tbx::Uuid, tbx::GraphicsPipelineDesc> _pipeline_descs = {};
        std::unordered_map<tbx::Uuid, GLuint> _pipeline_vertex_arrays = {};
        std::unordered_map<tbx::Uuid, OpenGlSampler> _samplers = {};
        std::unordered_map<tbx::Uuid, OpenGlTexture> _textures = {};
        std::unordered_map<tbx::Uuid, tbx::GraphicsTextureDesc> _texture_descs = {};
        std::unordered_map<tbx::Window, OpenGlContext> _contexts = {};
        std::unique_ptr<OpenGlFramebuffer> _pass_framebuffer = {};
        tbx::Window _active_window = {};
        tbx::Uuid _current_pipeline = {};
        bool _is_gl_loaded = false;
        bool _is_initialized = false;
        bool _is_pass_active = false;
    };
}
