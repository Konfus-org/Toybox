#include "opengl_graphics_backend.h"
#include "opengl_resources/opengl_bindless.h"
#include "opengl_resources/opengl_shader.h"
#include "tbx/debugging/macros.h"
#include <cstddef>
#include <cstdint>
#include <glad/glad.h>
#include <string_view>
#include <utility>
#include <vector>

namespace opengl_rendering
{
    struct OpenGlGraphicsBackend::OpenGlBufferResource
    {
        ~OpenGlBufferResource() noexcept
        {
            if (id != 0U)
                glDeleteBuffers(1, &id);
        }

        uint32 id = 0U;
        tbx::GraphicsBufferDesc desc = {};
    };

    struct OpenGlGraphicsBackend::OpenGlTextureResource
    {
        ~OpenGlTextureResource() noexcept
        {
            if (id != 0U)
                glDeleteTextures(1, &id);
        }

        uint32 id = 0U;
        tbx::GraphicsTextureDesc desc = {};
    };

    struct OpenGlGraphicsBackend::OpenGlSamplerResource
    {
        ~OpenGlSamplerResource() noexcept
        {
            if (id != 0U)
                glDeleteSamplers(1, &id);
        }

        uint32 id = 0U;
    };

    struct OpenGlGraphicsBackend::OpenGlPipelineResource
    {
        ~OpenGlPipelineResource() noexcept
        {
            if (vertex_array_id != 0U)
                glDeleteVertexArrays(1, &vertex_array_id);
        }

        std::shared_ptr<OpenGlShaderProgram> program = nullptr;
        tbx::GraphicsPipelineDesc desc = {};
        uint32 vertex_array_id = 0U;
    };

    static GLenum to_gl_primitive_type(const tbx::GraphicsPrimitiveType type)
    {
        switch (type)
        {
            case tbx::GraphicsPrimitiveType::TRIANGLES:
                return GL_TRIANGLES;
            case tbx::GraphicsPrimitiveType::LINES:
                return GL_LINES;
            case tbx::GraphicsPrimitiveType::POINTS:
                return GL_POINTS;
            default:
                return GL_TRIANGLES;
        }
    }

    static GLenum to_gl_index_type(const tbx::GraphicsIndexType type)
    {
        switch (type)
        {
            case tbx::GraphicsIndexType::UINT16:
                return GL_UNSIGNED_SHORT;
            case tbx::GraphicsIndexType::UINT32:
                return GL_UNSIGNED_INT;
            default:
                return GL_UNSIGNED_INT;
        }
    }

    static uint32 get_index_size(const tbx::GraphicsIndexType type)
    {
        switch (type)
        {
            case tbx::GraphicsIndexType::UINT16:
                return sizeof(uint16);
            case tbx::GraphicsIndexType::UINT32:
                return sizeof(uint32);
            default:
                return sizeof(uint32);
        }
    }

    static GLenum to_gl_buffer_usage(const tbx::GraphicsBufferDesc& desc)
    {
        return desc.is_dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;
    }

    static GLenum to_gl_texture_internal_format(const tbx::GraphicsTextureFormat format)
    {
        switch (format)
        {
            case tbx::GraphicsTextureFormat::RGBA8:
                return GL_RGBA8;
            case tbx::GraphicsTextureFormat::RGBA16_FLOAT:
                return GL_RGBA16F;
            case tbx::GraphicsTextureFormat::RGBA32_FLOAT:
                return GL_RGBA32F;
            case tbx::GraphicsTextureFormat::DEPTH24_STENCIL8:
                return GL_DEPTH24_STENCIL8;
            case tbx::GraphicsTextureFormat::DEPTH32_FLOAT:
                return GL_DEPTH_COMPONENT32F;
            default:
                return GL_RGBA8;
        }
    }

    static GLenum to_gl_texture_upload_format(const tbx::GraphicsTextureFormat format)
    {
        switch (format)
        {
            case tbx::GraphicsTextureFormat::DEPTH24_STENCIL8:
                return GL_DEPTH_STENCIL;
            case tbx::GraphicsTextureFormat::DEPTH32_FLOAT:
                return GL_DEPTH_COMPONENT;
            default:
                return GL_RGBA;
        }
    }

    static GLenum to_gl_texture_upload_type(const tbx::GraphicsTextureFormat format)
    {
        switch (format)
        {
            case tbx::GraphicsTextureFormat::RGBA16_FLOAT:
            case tbx::GraphicsTextureFormat::RGBA32_FLOAT:
            case tbx::GraphicsTextureFormat::DEPTH32_FLOAT:
                return GL_FLOAT;
            case tbx::GraphicsTextureFormat::DEPTH24_STENCIL8:
                return GL_UNSIGNED_INT_24_8;
            default:
                return GL_UNSIGNED_BYTE;
        }
    }

    static GLenum to_gl_vertex_format_type(const tbx::GraphicsVertexFormat format)
    {
        switch (format)
        {
            case tbx::GraphicsVertexFormat::UINT32:
                return GL_UNSIGNED_INT;
            case tbx::GraphicsVertexFormat::INT32:
                return GL_INT;
            default:
                return GL_FLOAT;
        }
    }

    static uint32 get_vertex_format_component_count(const tbx::GraphicsVertexFormat format)
    {
        switch (format)
        {
            case tbx::GraphicsVertexFormat::VEC2:
                return 2U;
            case tbx::GraphicsVertexFormat::VEC3:
                return 3U;
            case tbx::GraphicsVertexFormat::VEC4:
                return 4U;
            default:
                return 1U;
        }
    }

    static bool is_integer_vertex_format(const tbx::GraphicsVertexFormat format)
    {
        return format == tbx::GraphicsVertexFormat::UINT32
               || format == tbx::GraphicsVertexFormat::INT32;
    }

    static GLenum get_framebuffer_attachment(
        const std::size_t color_target_index,
        const tbx::GraphicsTextureFormat format)
    {
        if (format == tbx::GraphicsTextureFormat::DEPTH24_STENCIL8)
            return GL_DEPTH_STENCIL_ATTACHMENT;
        if (format == tbx::GraphicsTextureFormat::DEPTH32_FLOAT)
            return GL_DEPTH_ATTACHMENT;
        return static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + color_target_index);
    }

    static GLbitfield to_gl_clear_flags(const tbx::GraphicsClearFlags flags)
    {
        auto clear_flags = GLbitfield {0U};
        const auto value = static_cast<uint8>(flags);
        if ((value & static_cast<uint8>(tbx::GraphicsClearFlags::COLOR)) != 0U)
            clear_flags |= GL_COLOR_BUFFER_BIT;
        if ((value & static_cast<uint8>(tbx::GraphicsClearFlags::DEPTH)) != 0U)
            clear_flags |= GL_DEPTH_BUFFER_BIT;
        if ((value & static_cast<uint8>(tbx::GraphicsClearFlags::STENCIL)) != 0U)
            clear_flags |= GL_STENCIL_BUFFER_BIT;
        return clear_flags;
    }

    static void gl_message_callback(
        GLenum source,
        GLenum type,
        GLuint id,
        GLenum severity,
        GLsizei length,
        const GLchar* message,
        const void*)
    {
        if (severity == GL_DEBUG_SEVERITY_NOTIFICATION)
            return;

        TBX_TRACE_WARNING(
            "GL debug message (source: {}, type: {}, id: {}, severity: {}) - {}",
            source,
            type,
            id,
            severity,
            std::string_view(message, static_cast<std::size_t>(length)));
    }

    OpenGlGraphicsBackend::OpenGlGraphicsBackend(tbx::IOpenGlContextManager& context_manager)
        : _context_manager(context_manager)
    {
    }

    OpenGlGraphicsBackend::~OpenGlGraphicsBackend() noexcept
    {
        shutdown();
    }

    tbx::Result OpenGlGraphicsBackend::initialize(const tbx::GraphicsSettings& settings)
    {
        bool debug_context_enabled = false;
#if defined(TBX_DEBUG)
        debug_context_enabled = true;
#endif

        _context_manager.initialize(
            4,
            5,
            24,
            8,
            true,
            debug_context_enabled,
            settings.vsync_enabled);
        _is_initialized = true;
        return {};
    }

    void OpenGlGraphicsBackend::shutdown()
    {
        if (!_contexts.empty())
            _contexts.begin()->second->make_current();

        if (_pass_framebuffer != 0U)
        {
            glDeleteFramebuffers(1, &_pass_framebuffer);
            _pass_framebuffer = 0U;
        }

        _active_pipeline = nullptr;
        _active_context = nullptr;
        _pipelines.clear();
        _samplers.clear();
        _textures.clear();
        _buffers.clear();

        auto windows = std::vector<tbx::Window> {};
        windows.reserve(_contexts.size());
        for (const auto& entry : _contexts)
            windows.push_back(entry.first);
        for (const auto& window : windows)
            _context_manager.destroy_context(window);
        _contexts.clear();

        if (_is_initialized)
            _context_manager.shutdown();
        _is_initialized = false;
        _is_gl_loaded = false;
    }

    tbx::GraphicsApi OpenGlGraphicsBackend::get_api() const
    {
        return tbx::GraphicsApi::OPEN_GL;
    }

    void OpenGlGraphicsBackend::wait_for_idle()
    {
        if (_is_gl_loaded)
            glFinish();
    }

    tbx::Result OpenGlGraphicsBackend::begin_frame(const tbx::GraphicsFrameInfo& frame)
    {
        if (!_is_initialized)
            return make_failure("OpenGL rendering: backend has not been initialized.");
        if (const auto result = ensure_context(frame.output_window); !result)
            return result;
        if (const auto result = ensure_gl_initialized(); !result)
            return result;

        _active_window = frame.output_window;
        _active_pipeline = nullptr;
        return {};
    }

    tbx::Result OpenGlGraphicsBackend::begin_view(const tbx::GraphicsView& view)
    {
        return set_viewport(view.viewport);
    }

    tbx::Result OpenGlGraphicsBackend::end_frame()
    {
        _active_pipeline = nullptr;
        return {};
    }

    tbx::Result OpenGlGraphicsBackend::end_view()
    {
        return {};
    }

    tbx::Result OpenGlGraphicsBackend::present()
    {
        if (!_active_context)
            return make_failure("OpenGL rendering: cannot present without an active context.");
        return _active_context->present();
    }

    tbx::Result OpenGlGraphicsBackend::begin_pass(const tbx::GraphicsPassDesc& pass)
    {
        if (!_is_gl_loaded)
            return make_failure("OpenGL rendering: cannot begin a pass before GL is initialized.");

        if (!pass.color_targets.empty() || pass.depth_stencil_target.is_valid())
        {
            if (_pass_framebuffer == 0U)
                glCreateFramebuffers(1, &_pass_framebuffer);

            for (uint32 attachment_index = 0U; attachment_index < 8U; ++attachment_index)
            {
                glNamedFramebufferTexture(
                    _pass_framebuffer,
                    GL_COLOR_ATTACHMENT0 + attachment_index,
                    0U,
                    0);
            }
            glNamedFramebufferTexture(_pass_framebuffer, GL_DEPTH_ATTACHMENT, 0U, 0);
            glNamedFramebufferTexture(_pass_framebuffer, GL_DEPTH_STENCIL_ATTACHMENT, 0U, 0);

            auto draw_buffers = std::vector<GLenum> {};
            draw_buffers.reserve(pass.color_targets.size());
            for (std::size_t i = 0U; i < pass.color_targets.size(); ++i)
            {
                auto* texture = try_get_texture(pass.color_targets[i]);
                if (!texture)
                    return make_failure("OpenGL rendering: color target texture is missing.");

                const auto attachment = get_framebuffer_attachment(i, texture->desc.format);
                glNamedFramebufferTexture(_pass_framebuffer, attachment, texture->id, 0);
                draw_buffers.push_back(attachment);
            }

            if (pass.depth_stencil_target.is_valid())
            {
                auto* texture = try_get_texture(pass.depth_stencil_target);
                if (!texture)
                    return make_failure("OpenGL rendering: depth target texture is missing.");

                const auto attachment = get_framebuffer_attachment(0U, texture->desc.format);
                glNamedFramebufferTexture(_pass_framebuffer, attachment, texture->id, 0);
            }

            if (!draw_buffers.empty())
            {
                glNamedFramebufferDrawBuffers(
                    _pass_framebuffer,
                    static_cast<GLsizei>(draw_buffers.size()),
                    draw_buffers.data());
                glNamedFramebufferReadBuffer(_pass_framebuffer, draw_buffers.front());
            }
            else
            {
                const auto draw_buffer = GLenum {GL_NONE};
                glNamedFramebufferDrawBuffers(_pass_framebuffer, 1, &draw_buffer);
                glNamedFramebufferReadBuffer(_pass_framebuffer, GL_NONE);
            }

            const auto framebuffer_status =
                glCheckNamedFramebufferStatus(_pass_framebuffer, GL_FRAMEBUFFER);
            if (framebuffer_status != GL_FRAMEBUFFER_COMPLETE)
                return make_failure("OpenGL rendering: render pass framebuffer is incomplete.");

            glBindFramebuffer(GL_FRAMEBUFFER, _pass_framebuffer);
        }
        else
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0U);
        }

        glClearColor(pass.clear_color.r, pass.clear_color.g, pass.clear_color.b, pass.clear_color.a);
        glClearDepth(pass.clear_depth);
        glClearStencil(static_cast<GLint>(pass.clear_stencil));
        const auto clear_flags = to_gl_clear_flags(pass.clear_flags);
        if (clear_flags != 0U)
            glClear(clear_flags);
        return {};
    }

    tbx::Result OpenGlGraphicsBackend::end_pass()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0U);
        return {};
    }

    tbx::Result OpenGlGraphicsBackend::set_viewport(const tbx::Viewport& viewport)
    {
        glViewport(
            static_cast<GLint>(viewport.position.x),
            static_cast<GLint>(viewport.position.y),
            static_cast<GLsizei>(viewport.dimensions.width),
            static_cast<GLsizei>(viewport.dimensions.height));
        return {};
    }

    tbx::Result OpenGlGraphicsBackend::set_scissor(const tbx::Viewport& scissor)
    {
        glEnable(GL_SCISSOR_TEST);
        glScissor(
            static_cast<GLint>(scissor.position.x),
            static_cast<GLint>(scissor.position.y),
            static_cast<GLsizei>(scissor.dimensions.width),
            static_cast<GLsizei>(scissor.dimensions.height));
        return {};
    }

    tbx::Result OpenGlGraphicsBackend::bind_pipeline(const tbx::Uuid& pipeline_resource_uuid)
    {
        auto* pipeline = try_get_pipeline(pipeline_resource_uuid);
        if (!pipeline || !pipeline->program)
            return make_failure("OpenGL rendering: pipeline resource is missing.");

        _active_pipeline = pipeline;
        glBindVertexArray(pipeline->vertex_array_id);
        pipeline->program->bind();

        pipeline->desc.is_depth_test_enabled ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
        glDepthMask(pipeline->desc.is_depth_write_enabled ? GL_TRUE : GL_FALSE);
        pipeline->desc.is_blending_enabled ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
        pipeline->desc.is_culling_enabled ? glEnable(GL_CULL_FACE) : glDisable(GL_CULL_FACE);
        return {};
    }

    tbx::Result OpenGlGraphicsBackend::bind_vertex_buffer(
        const uint32 slot,
        const tbx::Uuid& buffer_resource_uuid)
    {
        auto* buffer = try_get_buffer(buffer_resource_uuid);
        if (!buffer)
            return make_failure("OpenGL rendering: vertex buffer resource is missing.");
        if (!_active_pipeline)
            return make_failure("OpenGL rendering: cannot bind a vertex buffer without a pipeline.");

        const auto stride = get_vertex_stride(slot);
        if (stride == 0U)
            return make_failure("OpenGL rendering: vertex buffer slot has no pipeline layout.");

        glVertexArrayVertexBuffer(
            _active_pipeline->vertex_array_id,
            slot,
            buffer->id,
            0,
            static_cast<GLsizei>(stride));
        return {};
    }

    tbx::Result OpenGlGraphicsBackend::bind_index_buffer(
        const tbx::Uuid& buffer_resource_uuid,
        const tbx::GraphicsIndexType index_type)
    {
        auto* buffer = try_get_buffer(buffer_resource_uuid);
        if (!buffer)
            return make_failure("OpenGL rendering: index buffer resource is missing.");
        if (!_active_pipeline)
            return make_failure("OpenGL rendering: cannot bind an index buffer without a pipeline.");

        _active_index_type = index_type;
        glVertexArrayElementBuffer(_active_pipeline->vertex_array_id, buffer->id);
        return {};
    }

    tbx::Result OpenGlGraphicsBackend::bind_uniform_buffer(
        const uint32 slot,
        const tbx::Uuid& buffer_resource_uuid)
    {
        auto* buffer = try_get_buffer(buffer_resource_uuid);
        if (!buffer)
            return make_failure("OpenGL rendering: uniform buffer resource is missing.");
        glBindBufferBase(GL_UNIFORM_BUFFER, slot, buffer->id);
        return {};
    }

    tbx::Result OpenGlGraphicsBackend::bind_storage_buffer(
        const uint32 slot,
        const tbx::Uuid& buffer_resource_uuid)
    {
        auto* buffer = try_get_buffer(buffer_resource_uuid);
        if (!buffer)
            return make_failure("OpenGL rendering: storage buffer resource is missing.");
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, slot, buffer->id);
        return {};
    }

    tbx::Result OpenGlGraphicsBackend::bind_texture(
        const uint32 slot,
        const tbx::Uuid& texture_resource_uuid)
    {
        auto* texture = try_get_texture(texture_resource_uuid);
        if (!texture)
            return make_failure("OpenGL rendering: texture resource is missing.");
        glBindTextureUnit(slot, texture->id);
        return {};
    }

    tbx::Result OpenGlGraphicsBackend::bind_sampler(
        const uint32 slot,
        const tbx::Uuid& sampler_resource_uuid)
    {
        auto* sampler = try_get_sampler(sampler_resource_uuid);
        if (!sampler)
            return make_failure("OpenGL rendering: sampler resource is missing.");
        glBindSampler(slot, sampler->id);
        return {};
    }

    tbx::Result OpenGlGraphicsBackend::draw(const uint32 vertex_count, const uint32 vertex_offset)
    {
        if (!_active_pipeline)
            return make_failure("OpenGL rendering: cannot draw without a pipeline.");

        glDrawArrays(
            to_gl_primitive_type(_active_pipeline->desc.primitive_type),
            static_cast<GLint>(vertex_offset),
            static_cast<GLsizei>(vertex_count));
        return {};
    }

    tbx::Result OpenGlGraphicsBackend::draw_indexed(const tbx::GraphicsDrawIndexedDesc& draw)
    {
        if (!_active_pipeline)
            return make_failure("OpenGL rendering: cannot draw indexed without a pipeline.");

        const auto index_type = to_gl_index_type(draw.index_type);
        const auto index_offset =
            reinterpret_cast<void*>(static_cast<std::uintptr_t>(
                draw.index_offset * get_index_size(draw.index_type)));
        glDrawElementsInstancedBaseVertexBaseInstance(
            to_gl_primitive_type(draw.primitive_type),
            static_cast<GLsizei>(draw.index_count),
            index_type,
            index_offset,
            static_cast<GLsizei>(draw.instance_count),
            draw.vertex_offset,
            draw.first_instance);
        return {};
    }

    tbx::Result OpenGlGraphicsBackend::unload(const tbx::Uuid& resource_uuid)
    {
        _buffers.erase(resource_uuid);
        _textures.erase(resource_uuid);
        _samplers.erase(resource_uuid);
        _pipelines.erase(resource_uuid);
        return {};
    }

    tbx::Result OpenGlGraphicsBackend::upload_buffer(
        const tbx::GraphicsBufferDesc& desc,
        const void* data,
        const uint64 data_size,
        tbx::Uuid& out_resource_uuid)
    {
        auto resource = std::make_unique<OpenGlBufferResource>();
        resource->desc = desc;
        glCreateBuffers(1, &resource->id);
        glNamedBufferData(
            resource->id,
            static_cast<GLsizeiptr>(desc.size != 0U ? desc.size : data_size),
            data,
            to_gl_buffer_usage(desc));

        out_resource_uuid = tbx::Uuid::generate();
        _buffers.insert_or_assign(out_resource_uuid, std::move(resource));
        (void)data_size;
        return {};
    }

    tbx::Result OpenGlGraphicsBackend::upload_pipeline(
        const tbx::GraphicsPipelineDesc& desc,
        tbx::Uuid& out_resource_uuid)
    {
        if (desc.shader.sources.empty())
            return make_failure("OpenGL rendering: pipeline requires at least one shader stage.");

        auto shader_resources = std::vector<std::shared_ptr<OpenGlShader>> {};
        shader_resources.reserve(desc.shader.sources.size());
        for (const auto& source : desc.shader.sources)
        {
            auto shader = std::make_shared<OpenGlShader>(source);
            if (!shader->compile())
                return make_failure("OpenGL rendering: failed to compile pipeline shader.");
            shader_resources.push_back(std::move(shader));
        }

        auto resource = std::make_unique<OpenGlPipelineResource>();
        resource->desc = desc;
        resource->program = std::make_shared<OpenGlShaderProgram>(shader_resources);
        if (!resource->program || resource->program->get_program_id() == 0U)
            return make_failure("OpenGL rendering: failed to link pipeline shader program.");

        glCreateVertexArrays(1, &resource->vertex_array_id);
        for (const auto& layout : desc.vertex_buffers)
            glVertexArrayBindingDivisor(
                resource->vertex_array_id,
                layout.slot,
                layout.is_per_instance ? 1U : 0U);

        for (const auto& attribute : desc.vertex_attributes)
        {
            glEnableVertexArrayAttrib(resource->vertex_array_id, attribute.location);
            if (is_integer_vertex_format(attribute.format))
            {
                glVertexArrayAttribIFormat(
                    resource->vertex_array_id,
                    attribute.location,
                    get_vertex_format_component_count(attribute.format),
                    to_gl_vertex_format_type(attribute.format),
                    attribute.offset);
            }
            else
            {
                glVertexArrayAttribFormat(
                    resource->vertex_array_id,
                    attribute.location,
                    get_vertex_format_component_count(attribute.format),
                    to_gl_vertex_format_type(attribute.format),
                    GL_FALSE,
                    attribute.offset);
            }
            glVertexArrayAttribBinding(
                resource->vertex_array_id,
                attribute.location,
                attribute.buffer_slot);
        }

        out_resource_uuid = tbx::Uuid::generate();
        _pipelines.insert_or_assign(out_resource_uuid, std::move(resource));
        return {};
    }

    tbx::Result OpenGlGraphicsBackend::upload_sampler(
        const tbx::GraphicsSamplerDesc& desc,
        tbx::Uuid& out_resource_uuid)
    {
        auto resource = std::make_unique<OpenGlSamplerResource>();
        glCreateSamplers(1, &resource->id);
        glSamplerParameteri(
            resource->id,
            GL_TEXTURE_MIN_FILTER,
            desc.is_linear_filtering_enabled
                ? (desc.is_mipmapping_enabled ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR)
                : (desc.is_mipmapping_enabled ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST));
        glSamplerParameteri(
            resource->id,
            GL_TEXTURE_MAG_FILTER,
            desc.is_linear_filtering_enabled ? GL_LINEAR : GL_NEAREST);
        glSamplerParameteri(
            resource->id,
            GL_TEXTURE_WRAP_S,
            desc.is_repeating ? GL_REPEAT : GL_CLAMP_TO_EDGE);
        glSamplerParameteri(
            resource->id,
            GL_TEXTURE_WRAP_T,
            desc.is_repeating ? GL_REPEAT : GL_CLAMP_TO_EDGE);

        out_resource_uuid = tbx::Uuid::generate();
        _samplers.insert_or_assign(out_resource_uuid, std::move(resource));
        return {};
    }

    tbx::Result OpenGlGraphicsBackend::upload_texture(
        const tbx::GraphicsTextureDesc& desc,
        const void* data,
        const uint64 data_size,
        tbx::Uuid& out_resource_uuid)
    {
        auto resource = std::make_unique<OpenGlTextureResource>();
        resource->desc = desc;
        glCreateTextures(GL_TEXTURE_2D, 1, &resource->id);
        glTextureStorage2D(
            resource->id,
            static_cast<GLsizei>(desc.mip_count),
            to_gl_texture_internal_format(desc.format),
            static_cast<GLsizei>(desc.size.width),
            static_cast<GLsizei>(desc.size.height));

        if (data != nullptr && data_size > 0U)
        {
            glTextureSubImage2D(
                resource->id,
                0,
                0,
                0,
                static_cast<GLsizei>(desc.size.width),
                static_cast<GLsizei>(desc.size.height),
                to_gl_texture_upload_format(desc.format),
                to_gl_texture_upload_type(desc.format),
                data);
        }

        glTextureParameteri(resource->id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(resource->id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(resource->id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(resource->id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        out_resource_uuid = tbx::Uuid::generate();
        _textures.insert_or_assign(out_resource_uuid, std::move(resource));
        return {};
    }

    tbx::Result OpenGlGraphicsBackend::update_settings(const tbx::GraphicsSettings& settings)
    {
        _context_manager.set_vsync(settings.vsync_enabled ? tbx::VsyncMode::ON : tbx::VsyncMode::OFF);
        return {};
    }

    tbx::Result OpenGlGraphicsBackend::update_buffer(
        const tbx::Uuid& resource_uuid,
        const void* data,
        const uint64 data_size,
        const uint64 offset)
    {
        auto* buffer = try_get_buffer(resource_uuid);
        if (!buffer)
            return make_failure("OpenGL rendering: buffer resource is missing.");

        glNamedBufferSubData(
            buffer->id,
            static_cast<GLintptr>(offset),
            static_cast<GLsizeiptr>(data_size),
            data);
        return {};
    }

    tbx::Result OpenGlGraphicsBackend::update_texture(
        const tbx::Uuid& resource_uuid,
        const tbx::GraphicsTextureUpdateDesc& desc,
        const void* data,
        const uint64 data_size)
    {
        auto* texture = try_get_texture(resource_uuid);
        if (!texture)
            return make_failure("OpenGL rendering: texture resource is missing.");
        if (data == nullptr || data_size == 0U)
            return {};

        glTextureSubImage2D(
            texture->id,
            static_cast<GLint>(desc.mip_level),
            static_cast<GLint>(desc.x),
            static_cast<GLint>(desc.y),
            static_cast<GLsizei>(desc.width),
            static_cast<GLsizei>(desc.height),
            to_gl_texture_upload_format(texture->desc.format),
            to_gl_texture_upload_type(texture->desc.format),
            data);
        return {};
    }

    tbx::Result OpenGlGraphicsBackend::ensure_context(const tbx::Window& window)
    {
        if (!window.is_valid())
            return make_failure("OpenGL rendering: frame output window is invalid.");

        if (const auto existing = _contexts.find(window); existing != _contexts.end())
        {
            _active_context = existing->second.get();
            return _active_context->make_current();
        }

        if (const auto create_result = _context_manager.create_context(window); !create_result)
            return create_result;

        auto context = std::make_unique<OpenGlContext>(_context_manager, window);
        _active_context = context.get();
        _contexts.insert_or_assign(window, std::move(context));
        return _active_context->make_current();
    }

    tbx::Result OpenGlGraphicsBackend::ensure_gl_initialized()
    {
        if (_is_gl_loaded)
            return {};

        auto* loader = _context_manager.get_proc_address();
        if (loader == nullptr)
            return make_failure("OpenGL rendering: context manager returned a null GL loader.");

        const auto load_result = gladLoadGLLoader(loader);
        if (load_result == 0)
            return make_failure("OpenGL rendering: failed to initialize GLAD.");

        set_bindless_proc_loader(loader);

#if defined(TBX_DEBUG)
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageControl(
            GL_DONT_CARE,
            GL_DONT_CARE,
            GL_DEBUG_SEVERITY_NOTIFICATION,
            0,
            nullptr,
            GL_FALSE);
        glDebugMessageCallback(gl_message_callback, nullptr);
#endif

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
        glDisable(GL_BLEND);
        _is_gl_loaded = true;
        return {};
    }

    tbx::Result OpenGlGraphicsBackend::make_failure(const char* message) const
    {
        auto result = tbx::Result {};
        result.flag_failure(message);
        return result;
    }

    OpenGlGraphicsBackend::OpenGlBufferResource* OpenGlGraphicsBackend::try_get_buffer(
        const tbx::Uuid& resource_uuid) const
    {
        const auto it = _buffers.find(resource_uuid);
        return it != _buffers.end() ? it->second.get() : nullptr;
    }

    OpenGlGraphicsBackend::OpenGlTextureResource* OpenGlGraphicsBackend::try_get_texture(
        const tbx::Uuid& resource_uuid) const
    {
        const auto it = _textures.find(resource_uuid);
        return it != _textures.end() ? it->second.get() : nullptr;
    }

    OpenGlGraphicsBackend::OpenGlSamplerResource* OpenGlGraphicsBackend::try_get_sampler(
        const tbx::Uuid& resource_uuid) const
    {
        const auto it = _samplers.find(resource_uuid);
        return it != _samplers.end() ? it->second.get() : nullptr;
    }

    OpenGlGraphicsBackend::OpenGlPipelineResource* OpenGlGraphicsBackend::try_get_pipeline(
        const tbx::Uuid& resource_uuid) const
    {
        const auto it = _pipelines.find(resource_uuid);
        return it != _pipelines.end() ? it->second.get() : nullptr;
    }

    uint32 OpenGlGraphicsBackend::get_vertex_stride(const uint32 slot) const
    {
        if (!_active_pipeline)
            return 0U;

        for (const auto& layout : _active_pipeline->desc.vertex_buffers)
            if (layout.slot == slot)
                return layout.stride;
        return 0U;
    }
}
