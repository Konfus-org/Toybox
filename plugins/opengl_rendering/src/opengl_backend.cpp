#include "opengl_backend.h"
#include "opengl_resources/opengl_utils.h"
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace opengl_rendering
{
    static void apply_pipeline_state(const tbx::GraphicsPipelineDesc& desc)
    {
        desc.is_depth_test_enabled ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
        glDepthMask(desc.is_depth_write_enabled ? GL_TRUE : GL_FALSE);
        desc.is_blending_enabled ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
        desc.is_culling_enabled ? glEnable(GL_CULL_FACE) : glDisable(GL_CULL_FACE);

        if (desc.is_blending_enabled)
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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
        const bool vsync_enabled = settings.vsync_enabled;
        _context_manager.initialize(4, 5, 24, 8, true, false, vsync_enabled);
        _is_initialized = true;
        return make_success();
    }

    void OpenGlGraphicsBackend::shutdown()
    {
        if (_is_gl_loaded)
            destroy_resources();

        auto windows = std::vector<tbx::Window> {};
        windows.reserve(_contexts.size());
        for (const auto& [window, context] : _contexts)
        {
            (void)context;
            windows.push_back(window);
        }
        for (const auto& window : windows)
            _context_manager.destroy_context(window);

        _contexts.clear();
        _active_window = {};
        _is_initialized = false;
        _is_gl_loaded = false;
        _is_pass_active = false;
    }

    tbx::GraphicsApi OpenGlGraphicsBackend::get_api() const
    {
        return tbx::GraphicsApi::OPEN_GL;
    }

    tbx::Result OpenGlGraphicsBackend::begin_frame(const tbx::GraphicsFrameInfo& frame)
    {
        if (!_is_initialized)
            return make_failure("OpenGL backend: initialize must be called before begin_frame.");

        if (!frame.output_window.is_valid())
            return make_failure("OpenGL backend: frame output window is invalid.");

        if (auto result = ensure_frame_context(frame.output_window); !result)
            return result;

        clear_bound_state();

        if (frame.output_resolution.width > 0U && frame.output_resolution.height > 0U)
            glViewport(
                0,
                0,
                static_cast<GLsizei>(frame.output_resolution.width),
                static_cast<GLsizei>(frame.output_resolution.height));

        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::begin_view(const tbx::GraphicsView& view)
    {
        return set_viewport(view.viewport);
    }

    tbx::Result OpenGlGraphicsBackend::end_frame()
    {
        clear_bound_state();
        _active_window = {};
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::end_view()
    {
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::present()
    {
        if (!_active_window.is_valid())
            return make_failure("OpenGL backend: no active window to present.");

        const auto context_it = _contexts.find(_active_window);
        if (context_it == _contexts.end())
            return make_failure("OpenGL backend: active window context was not found.");

        return context_it->second.present();
    }

    void OpenGlGraphicsBackend::wait_for_idle()
    {
        glFinish();
    }

    tbx::Result OpenGlGraphicsBackend::begin_pass(const tbx::GraphicsPassDesc& pass)
    {
        if (_is_pass_active)
            return make_failure("OpenGL backend: a render pass is already active.");

        _pass_framebuffer.reset();

        if (!pass.color_targets.empty() || pass.depth_stencil_target.is_valid())
        {
            _pass_framebuffer = std::make_unique<OpenGlFramebuffer>();

            for (uint32 index = 0U; index < pass.color_targets.size(); ++index)
            {
                const auto texture_it = _textures.find(pass.color_targets[index]);
                if (texture_it == _textures.end())
                    return make_failure("OpenGL backend: render pass color target was not found.");

                _pass_framebuffer->attach_color(index, texture_it->second);
            }

            if (pass.depth_stencil_target.is_valid())
            {
                const auto texture_it = _textures.find(pass.depth_stencil_target);
                if (texture_it == _textures.end())
                    return make_failure("OpenGL backend: render pass depth target was not found.");

                const auto desc_it = _texture_descs.find(pass.depth_stencil_target);
                if (desc_it == _texture_descs.end())
                    return make_failure(
                        "OpenGL backend: render pass depth target description was not found.");

                _pass_framebuffer->attach_depth_stencil(
                    texture_it->second,
                    desc_it->second.format);
            }

            _pass_framebuffer->set_draw_buffers(static_cast<uint32>(pass.color_targets.size()));

            if (!_pass_framebuffer->is_complete())
                return make_failure("OpenGL backend: render pass framebuffer is incomplete.");

            _pass_framebuffer->bind();
        }
        else
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0U);
        }

        GLbitfield clear_mask = 0U;
        if (has_clear_flag(pass.clear_flags, tbx::GraphicsClearFlags::COLOR))
        {
            glClearColor(
                pass.clear_color.r,
                pass.clear_color.g,
                pass.clear_color.b,
                pass.clear_color.a);
            clear_mask |= GL_COLOR_BUFFER_BIT;
        }
        if (has_clear_flag(pass.clear_flags, tbx::GraphicsClearFlags::DEPTH))
        {
            glClearDepth(pass.clear_depth);
            clear_mask |= GL_DEPTH_BUFFER_BIT;
        }
        if (has_clear_flag(pass.clear_flags, tbx::GraphicsClearFlags::STENCIL))
        {
            glClearStencil(static_cast<GLint>(pass.clear_stencil));
            clear_mask |= GL_STENCIL_BUFFER_BIT;
        }
        if (clear_mask != 0U)
            glClear(clear_mask);

        _is_pass_active = true;
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::end_pass()
    {
        if (!_is_pass_active)
            return make_failure("OpenGL backend: no render pass is active.");

        _is_pass_active = false;
        glBindFramebuffer(GL_FRAMEBUFFER, 0U);
        _pass_framebuffer.reset();
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::set_viewport(const tbx::Viewport& viewport)
    {
        glViewport(
            static_cast<GLint>(viewport.position.x),
            static_cast<GLint>(viewport.position.y),
            static_cast<GLsizei>(viewport.dimensions.width),
            static_cast<GLsizei>(viewport.dimensions.height));
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::set_scissor(const tbx::Viewport& scissor)
    {
        glEnable(GL_SCISSOR_TEST);
        glScissor(
            static_cast<GLint>(scissor.position.x),
            static_cast<GLint>(scissor.position.y),
            static_cast<GLsizei>(scissor.dimensions.width),
            static_cast<GLsizei>(scissor.dimensions.height));
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::bind_pipeline(const tbx::Uuid& pipeline_resource_uuid)
    {
        const auto program_it = _programs.find(pipeline_resource_uuid);
        if (program_it == _programs.end())
            return make_failure("OpenGL backend: pipeline was not found.");

        const auto vertex_array_it = _pipeline_vertex_arrays.find(pipeline_resource_uuid);
        const auto desc_it = _pipeline_descs.find(pipeline_resource_uuid);
        if (vertex_array_it == _pipeline_vertex_arrays.end()
            || desc_it == _pipeline_descs.end())
            return make_failure("OpenGL backend: pipeline state was not found.");

        program_it->second.bind();
        glBindVertexArray(vertex_array_it->second);
        apply_pipeline_state(desc_it->second);

        _current_pipeline = pipeline_resource_uuid;
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::bind_vertex_buffer(
        const uint32 slot,
        const tbx::Uuid& buffer_resource_uuid)
    {
        if (auto result = require_current_pipeline(); !result)
            return result;

        const auto buffer_it = _buffers.find(buffer_resource_uuid);
        if (buffer_it == _buffers.end())
            return make_failure("OpenGL backend: vertex buffer was not found.");
        const auto buffer_desc_it = _buffer_descs.find(buffer_resource_uuid);
        if (buffer_desc_it == _buffer_descs.end())
            return make_failure("OpenGL backend: vertex buffer description was not found.");
        if (auto result = require_buffer_usage(
                buffer_desc_it->second,
                tbx::GraphicsBufferUsage::VERTEX,
                "OpenGL backend: buffer is not a vertex buffer.");
            !result)
            return result;

        const auto& pipeline_desc = _pipeline_descs.at(_current_pipeline);
        const auto vertex_array = _pipeline_vertex_arrays.at(_current_pipeline);
        const auto* layout = find_vertex_buffer_layout(pipeline_desc, slot);
        if (!layout)
            return make_failure("OpenGL backend: vertex buffer slot is not described by pipeline.");

        glVertexArrayVertexBuffer(
            vertex_array,
            slot,
            buffer_it->second.get_buffer_id(),
            0,
            static_cast<GLsizei>(layout->stride));
        buffer_it->second.bind();

        for (const auto& attribute : pipeline_desc.vertex_attributes)
        {
            if (attribute.buffer_slot != slot)
                continue;

            glEnableVertexArrayAttrib(vertex_array, attribute.location);
            glVertexArrayAttribBinding(vertex_array, attribute.location, slot);

            if (is_integer_vertex_format(attribute.format))
            {
                glVertexArrayAttribIFormat(
                    vertex_array,
                    attribute.location,
                    get_vertex_component_count(attribute.format),
                    get_vertex_component_type(attribute.format),
                    attribute.offset);
            }
            else
            {
                glVertexArrayAttribFormat(
                    vertex_array,
                    attribute.location,
                    get_vertex_component_count(attribute.format),
                    get_vertex_component_type(attribute.format),
                    GL_FALSE,
                    attribute.offset);
            }

            glVertexArrayBindingDivisor(
                vertex_array,
                slot,
                layout->is_per_instance ? 1U : 0U);
        }

        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::bind_index_buffer(
        const tbx::Uuid& buffer_resource_uuid,
        const tbx::GraphicsIndexType index_type)
    {
        (void)index_type;

        if (auto result = require_current_pipeline(); !result)
            return result;

        const auto buffer_it = _buffers.find(buffer_resource_uuid);
        if (buffer_it == _buffers.end())
            return make_failure("OpenGL backend: index buffer was not found.");
        const auto buffer_desc_it = _buffer_descs.find(buffer_resource_uuid);
        if (buffer_desc_it == _buffer_descs.end())
            return make_failure("OpenGL backend: index buffer description was not found.");
        if (auto result = require_buffer_usage(
                buffer_desc_it->second,
                tbx::GraphicsBufferUsage::INDEX,
                "OpenGL backend: buffer is not an index buffer.");
            !result)
            return result;

        glVertexArrayElementBuffer(
            _pipeline_vertex_arrays.at(_current_pipeline),
            buffer_it->second.get_buffer_id());
        buffer_it->second.bind();
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::bind_uniform_buffer(
        const uint32 slot,
        const tbx::Uuid& buffer_resource_uuid)
    {
        const auto buffer_it = _buffers.find(buffer_resource_uuid);
        if (buffer_it == _buffers.end())
            return make_failure("OpenGL backend: uniform buffer was not found.");
        const auto buffer_desc_it = _buffer_descs.find(buffer_resource_uuid);
        if (buffer_desc_it == _buffer_descs.end())
            return make_failure("OpenGL backend: uniform buffer description was not found.");
        if (auto result = require_buffer_usage(
                buffer_desc_it->second,
                tbx::GraphicsBufferUsage::UNIFORM,
                "OpenGL backend: buffer is not a uniform buffer.");
            !result)
            return result;

        buffer_it->second.bind_slot(slot);
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::bind_storage_buffer(
        const uint32 slot,
        const tbx::Uuid& buffer_resource_uuid)
    {
        const auto buffer_it = _buffers.find(buffer_resource_uuid);
        if (buffer_it == _buffers.end())
            return make_failure("OpenGL backend: storage buffer was not found.");
        const auto buffer_desc_it = _buffer_descs.find(buffer_resource_uuid);
        if (buffer_desc_it == _buffer_descs.end())
            return make_failure("OpenGL backend: storage buffer description was not found.");
        if (auto result = require_buffer_usage(
                buffer_desc_it->second,
                tbx::GraphicsBufferUsage::STORAGE,
                "OpenGL backend: buffer is not a storage buffer.");
            !result)
            return result;

        buffer_it->second.bind_slot(slot);
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::bind_texture(
        const uint32 slot,
        const tbx::Uuid& texture_resource_uuid)
    {
        const auto texture_it = _textures.find(texture_resource_uuid);
        if (texture_it == _textures.end())
            return make_failure("OpenGL backend: texture was not found.");

        texture_it->second.bind_slot(slot);
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::bind_sampler(
        const uint32 slot,
        const tbx::Uuid& sampler_resource_uuid)
    {
        const auto sampler_it = _samplers.find(sampler_resource_uuid);
        if (sampler_it == _samplers.end())
            return make_failure("OpenGL backend: sampler was not found.");

        sampler_it->second.bind_slot(slot);
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::draw(const uint32 vertex_count, const uint32 vertex_offset)
    {
        if (auto result = require_current_pipeline(); !result)
            return result;

        const auto& pipeline_desc = _pipeline_descs.at(_current_pipeline);
        glDrawArrays(
            to_gl_primitive_type(pipeline_desc.primitive_type),
            static_cast<GLint>(vertex_offset),
            static_cast<GLsizei>(vertex_count));
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::draw_indexed(const tbx::GraphicsDrawIndexedDesc& draw)
    {
        if (auto result = require_current_pipeline(); !result)
            return result;

        const auto* index_offset =
            reinterpret_cast<const void*>(static_cast<std::uintptr_t>(draw.index_offset));
        glDrawElementsInstancedBaseVertexBaseInstance(
            to_gl_primitive_type(draw.primitive_type),
            static_cast<GLsizei>(draw.index_count),
            to_gl_index_type(draw.index_type),
            index_offset,
            static_cast<GLsizei>(draw.instance_count),
            draw.vertex_offset,
            draw.first_instance);
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::unload(const tbx::Uuid& resource_uuid)
    {
        if (auto buffer_it = _buffers.find(resource_uuid); buffer_it != _buffers.end())
        {
            _buffers.erase(buffer_it);
            _buffer_descs.erase(resource_uuid);
            return make_success();
        }

        if (auto program_it = _programs.find(resource_uuid); program_it != _programs.end())
        {
            if (auto vertex_array_it = _pipeline_vertex_arrays.find(resource_uuid);
                vertex_array_it != _pipeline_vertex_arrays.end())
            {
                glDeleteVertexArrays(1, &vertex_array_it->second);
                _pipeline_vertex_arrays.erase(vertex_array_it);
            }
            _pipeline_descs.erase(resource_uuid);
            _programs.erase(program_it);
            if (_current_pipeline == resource_uuid)
                _current_pipeline = {};
            return make_success();
        }

        if (auto sampler_it = _samplers.find(resource_uuid); sampler_it != _samplers.end())
        {
            _samplers.erase(sampler_it);
            return make_success();
        }

        if (auto texture_it = _textures.find(resource_uuid); texture_it != _textures.end())
        {
            _textures.erase(texture_it);
            _texture_descs.erase(resource_uuid);
            return make_success();
        }

        return make_failure("OpenGL backend: resource was not found.");
    }

    tbx::Result OpenGlGraphicsBackend::upload_buffer(
        const tbx::GraphicsBufferDesc& desc,
        const void* data,
        const uint64 data_size,
        tbx::Uuid& out_resource_uuid)
    {
        if (desc.size == 0U)
            return make_failure("OpenGL backend: buffer size must be greater than zero.");

        if (data_size > 0U && !data)
            return make_failure("OpenGL backend: buffer upload data is null.");

        if (data_size > desc.size)
            return make_failure("OpenGL backend: buffer upload data exceeds buffer size.");

        out_resource_uuid = tbx::Uuid::generate();
        _buffers.try_emplace(out_resource_uuid, desc, data, data_size);
        _buffer_descs.emplace(out_resource_uuid, desc);
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::upload_pipeline(
        const tbx::GraphicsPipelineDesc& desc,
        tbx::Uuid& out_resource_uuid)
    {
        auto shaders = std::vector<std::shared_ptr<OpenGlShader>> {};
        if (auto result = create_shaders(desc.shader, shaders); !result)
            return result;

        auto program = OpenGlShaderProgram(shaders);
        if (program.get_program_id() == 0U)
        {
            return make_failure("OpenGL backend: shader program link failed.");
        }

        auto vertex_array = GLuint {0U};
        glCreateVertexArrays(1, &vertex_array);

        out_resource_uuid = tbx::Uuid::generate();
        _programs.emplace(out_resource_uuid, std::move(program));
        _pipeline_descs.emplace(out_resource_uuid, desc);
        _pipeline_vertex_arrays.emplace(out_resource_uuid, vertex_array);
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::upload_sampler(
        const tbx::GraphicsSamplerDesc& desc,
        tbx::Uuid& out_resource_uuid)
    {
        out_resource_uuid = tbx::Uuid::generate();
        _samplers.emplace(out_resource_uuid, desc);
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::upload_texture(
        const tbx::GraphicsTextureDesc& desc,
        const void* data,
        const uint64 data_size,
        tbx::Uuid& out_resource_uuid)
    {
        if (desc.size.width == 0U || desc.size.height == 0U)
            return make_failure("OpenGL backend: texture size must be greater than zero.");

        if (data_size > 0U && !data)
            return make_failure("OpenGL backend: texture upload data is null.");

        if (data != nullptr && data_size < get_texture_byte_size(desc))
            return make_failure("OpenGL backend: texture upload data is smaller than texture size.");

        out_resource_uuid = tbx::Uuid::generate();
        _textures.try_emplace(out_resource_uuid, desc, data);
        _texture_descs.emplace(out_resource_uuid, desc);
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::update_settings(const tbx::GraphicsSettings& settings)
    {
        return _context_manager.set_vsync(
            settings.vsync_enabled ? tbx::VsyncMode::ON : tbx::VsyncMode::OFF);
    }

    tbx::Result OpenGlGraphicsBackend::update_buffer(
        const tbx::Uuid& resource_uuid,
        const void* data,
        const uint64 data_size,
        const uint64 offset)
    {
        const auto buffer_it = _buffers.find(resource_uuid);
        if (buffer_it == _buffers.end())
            return make_failure("OpenGL backend: buffer was not found.");
        const auto buffer_desc_it = _buffer_descs.find(resource_uuid);
        if (buffer_desc_it == _buffer_descs.end())
            return make_failure("OpenGL backend: buffer description was not found.");

        if (data_size > 0U && !data)
            return make_failure("OpenGL backend: buffer update data is null.");

        if (offset + data_size > buffer_desc_it->second.size)
            return make_failure("OpenGL backend: buffer update exceeds buffer size.");

        buffer_it->second.update(data, data_size, offset);
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::update_texture(
        const tbx::Uuid& resource_uuid,
        const tbx::GraphicsTextureUpdateDesc& desc,
        const void* data,
        const uint64 data_size)
    {
        const auto texture_it = _textures.find(resource_uuid);
        if (texture_it == _textures.end())
            return make_failure("OpenGL backend: texture was not found.");
        const auto texture_desc_it = _texture_descs.find(resource_uuid);
        if (texture_desc_it == _texture_descs.end())
            return make_failure("OpenGL backend: texture description was not found.");

        if (data_size > 0U && !data)
            return make_failure("OpenGL backend: texture update data is null.");

        const auto& texture_desc = texture_desc_it->second;
        if (desc.x + desc.width > texture_desc.size.width
            || desc.y + desc.height > texture_desc.size.height)
            return make_failure("OpenGL backend: texture update exceeds texture bounds.");

        const uint64 update_byte_size = static_cast<uint64>(desc.width)
                                        * static_cast<uint64>(desc.height)
                                        * get_texture_bytes_per_pixel(texture_desc.format);
        if (data != nullptr && data_size < update_byte_size)
            return make_failure("OpenGL backend: texture update data is smaller than region size.");

        texture_it->second.update(desc, texture_desc.format, data);
        return make_success();
    }

    void OpenGlGraphicsBackend::destroy_context(const tbx::Window& window)
    {
        const auto context_it = _contexts.find(window);
        if (context_it == _contexts.end())
            return;

        if (_active_window == window)
        {
            _active_window = {};
            _current_pipeline = {};
        }

        _context_manager.destroy_context(window);
        _contexts.erase(context_it);
    }

    void OpenGlGraphicsBackend::clear_bound_state()
    {
        _current_pipeline = {};
        glUseProgram(0U);
        glBindVertexArray(0U);
        glBindBuffer(GL_ARRAY_BUFFER, 0U);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0U);
    }

    void OpenGlGraphicsBackend::destroy_resources()
    {
        _pass_framebuffer.reset();
        for (auto& [pipeline_id, vertex_array] : _pipeline_vertex_arrays)
        {
            (void)pipeline_id;
            if (vertex_array != 0U)
                glDeleteVertexArrays(1, &vertex_array);
        }
        _pipeline_vertex_arrays.clear();
        _pipeline_descs.clear();
        _programs.clear();
        _buffers.clear();
        _buffer_descs.clear();
        _samplers.clear();
        _textures.clear();
        _texture_descs.clear();
        clear_bound_state();
    }

    tbx::Result OpenGlGraphicsBackend::ensure_frame_context(const tbx::Window& window)
    {
        const auto context_it = _contexts.find(window);
        if (context_it == _contexts.end())
        {
            if (auto result = _context_manager.create_context(window); !result)
                return result;

            auto [created_it, inserted] = _contexts.try_emplace(window, _context_manager, window);
            (void)inserted;
            if (auto result = created_it->second.make_current(); !result)
                return result;
        }
        else if (auto result = context_it->second.make_current(); !result)
        {
            return result;
        }

        _active_window = window;
        return ensure_gl_loaded();
    }

    tbx::Result OpenGlGraphicsBackend::ensure_gl_loaded()
    {
        if (_is_gl_loaded)
            return make_success();

        const auto loader = reinterpret_cast<GLADloadproc>(_context_manager.get_proc_address());
        if (!loader || gladLoadGLLoader(loader) == 0)
            return make_failure("OpenGL backend: failed to load OpenGL functions.");

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        _is_gl_loaded = true;
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::require_current_pipeline() const
    {
        if (!_current_pipeline.is_valid() || !_programs.contains(_current_pipeline)
            || !_pipeline_descs.contains(_current_pipeline)
            || !_pipeline_vertex_arrays.contains(_current_pipeline))
            return make_failure("OpenGL backend: no pipeline is currently bound.");

        return make_success();
    }
}
