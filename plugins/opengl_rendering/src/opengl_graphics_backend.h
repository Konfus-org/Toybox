#pragma once
#include "opengl_context.h"
#include "tbx/graphics/graphics_backend.h"
#include "tbx/graphics/opengl_context_manager.h"
#include <memory>
#include <unordered_map>

namespace opengl_rendering
{
    class OpenGlGraphicsBackend final : public tbx::IGraphicsBackend
    {
      public:
        OpenGlGraphicsBackend(tbx::IOpenGlContextManager& context_manager);
        ~OpenGlGraphicsBackend() noexcept override;

        tbx::Result initialize(const tbx::GraphicsSettings& settings) override;
        void shutdown() override;
        tbx::GraphicsApi get_api() const override;
        void wait_for_idle() override;
        tbx::Result begin_frame(const tbx::GraphicsFrameInfo& frame) override;
        tbx::Result begin_view(const tbx::GraphicsView& view) override;
        tbx::Result end_frame() override;
        tbx::Result end_view() override;
        tbx::Result present() override;
        tbx::Result begin_pass(const tbx::GraphicsPassDesc& pass) override;
        tbx::Result end_pass() override;
        tbx::Result set_viewport(const tbx::Viewport& viewport) override;
        tbx::Result set_scissor(const tbx::Viewport& scissor) override;
        tbx::Result bind_pipeline(const tbx::Uuid& pipeline_resource_uuid) override;
        tbx::Result bind_vertex_buffer(
            uint32 slot,
            const tbx::Uuid& buffer_resource_uuid) override;
        tbx::Result bind_index_buffer(
            const tbx::Uuid& buffer_resource_uuid,
            tbx::GraphicsIndexType index_type) override;
        tbx::Result bind_uniform_buffer(uint32 slot, const tbx::Uuid& buffer_resource_uuid) override;
        tbx::Result bind_storage_buffer(uint32 slot, const tbx::Uuid& buffer_resource_uuid) override;
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

      private:
        struct OpenGlBufferResource;
        struct OpenGlTextureResource;
        struct OpenGlSamplerResource;
        struct OpenGlPipelineResource;

        tbx::Result ensure_context(const tbx::Window& window);
        tbx::Result ensure_gl_initialized();
        tbx::Result make_failure(const char* message) const;
        OpenGlBufferResource* try_get_buffer(const tbx::Uuid& resource_uuid) const;
        OpenGlTextureResource* try_get_texture(const tbx::Uuid& resource_uuid) const;
        OpenGlSamplerResource* try_get_sampler(const tbx::Uuid& resource_uuid) const;
        OpenGlPipelineResource* try_get_pipeline(const tbx::Uuid& resource_uuid) const;
        uint32 get_vertex_stride(uint32 slot) const;

      private:
        tbx::IOpenGlContextManager& _context_manager;
        std::unordered_map<tbx::Window, std::unique_ptr<OpenGlContext>> _contexts = {};
        std::unordered_map<tbx::Uuid, std::unique_ptr<OpenGlBufferResource>> _buffers = {};
        std::unordered_map<tbx::Uuid, std::unique_ptr<OpenGlTextureResource>> _textures = {};
        std::unordered_map<tbx::Uuid, std::unique_ptr<OpenGlSamplerResource>> _samplers = {};
        std::unordered_map<tbx::Uuid, std::unique_ptr<OpenGlPipelineResource>> _pipelines = {};
        OpenGlContext* _active_context = nullptr;
        OpenGlPipelineResource* _active_pipeline = nullptr;
        tbx::Window _active_window = {};
        uint32 _pass_framebuffer = 0U;
        tbx::GraphicsIndexType _active_index_type = tbx::GraphicsIndexType::UINT32;
        bool _is_initialized = false;
        bool _is_gl_loaded = false;
    };
}
