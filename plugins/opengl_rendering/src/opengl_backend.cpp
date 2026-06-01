#include "opengl_backend.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/types/viewport.h"

namespace opengl_rendering
{
    static std::string gl_error_to_string(const GLenum error)
    {
        switch (error)
        {
            case GL_INVALID_ENUM:
                return "GL_INVALID_ENUM";
            case GL_INVALID_VALUE:
                return "GL_INVALID_VALUE";
            case GL_INVALID_OPERATION:
                return "GL_INVALID_OPERATION";
            case GL_INVALID_FRAMEBUFFER_OPERATION:
                return "GL_INVALID_FRAMEBUFFER_OPERATION";
            case GL_OUT_OF_MEMORY:
                return "GL_OUT_OF_MEMORY";
            case GL_STACK_UNDERFLOW:
                return "GL_STACK_UNDERFLOW";
            case GL_STACK_OVERFLOW:
                return "GL_STACK_OVERFLOW";
            default:
                return "0x" + std::to_string(static_cast<uint32>(error));
        }
    }

    static const char* get_gl_string(const GLenum name)
    {
        const auto* value = glGetString(name);
        return value ? reinterpret_cast<const char*>(value) : "unknown";
    }

    tbx::Result make_failure(std::string message)
    {
        auto result = tbx::Result {};
        result.flag_failure(std::move(message));
        return result;
    }

    tbx::Result make_success()
    {
        auto result = tbx::Result {};
        result.flag_success();
        return result;
    }

    tbx::Result consume_gl_errors(const std::string_view operation)
    {
        auto message = std::string {};
        for (GLenum error = glGetError(); error != GL_NO_ERROR; error = glGetError())
        {
            if (message.empty())
            {
                message = "OpenGL backend: ";
                message.append(operation);
                message.append(" failed with OpenGL error(s): ");
            }
            else
            {
                message.append(", ");
            }

            message.append(gl_error_to_string(error));
        }

        if (!message.empty())
            return make_failure(std::move(message));

        return make_success();
    }

    bool has_clear_flag(const tbx::GraphicsClearFlags value, const tbx::GraphicsClearFlags flag)
    {
        return (static_cast<uint8>(value) & static_cast<uint8>(flag)) != 0U;
    }

    bool is_integer_vertex_format(const tbx::GraphicsVertexFormat format)
    {
        return format == tbx::GraphicsVertexFormat::UINT32
               || format == tbx::GraphicsVertexFormat::INT32;
    }

    const tbx::GraphicsVertexBufferLayoutDesc* find_vertex_buffer_layout(
        const tbx::RasterPipelineDesc& desc,
        const uint32 slot)
    {
        const auto it = std::ranges::find_if(
            desc.vertex_buffers,
            [slot](const tbx::GraphicsVertexBufferLayoutDesc& layout)
            {
                return layout.slot == slot;
            });
        return it == desc.vertex_buffers.end() ? nullptr : &(*it);
    }

    GLenum to_gl_primitive_type(const tbx::GraphicsPrimitiveType primitive_type)
    {
        switch (primitive_type)
        {
            case tbx::GraphicsPrimitiveType::LINES:
                return GL_LINES;
            case tbx::GraphicsPrimitiveType::POINTS:
                return GL_POINTS;
            case tbx::GraphicsPrimitiveType::TRIANGLES:
            default:
                return GL_TRIANGLES;
        }
    }

    GLint get_vertex_component_count(const tbx::GraphicsVertexFormat format)
    {
        switch (format)
        {
            case tbx::GraphicsVertexFormat::VEC2:
                return 2;
            case tbx::GraphicsVertexFormat::VEC3:
                return 3;
            case tbx::GraphicsVertexFormat::VEC4:
                return 4;
            case tbx::GraphicsVertexFormat::FLOAT:
            case tbx::GraphicsVertexFormat::UINT32:
            case tbx::GraphicsVertexFormat::INT32:
            default:
                return 1;
        }
    }

    GLenum get_vertex_component_type(const tbx::GraphicsVertexFormat format)
    {
        switch (format)
        {
            case tbx::GraphicsVertexFormat::UINT32:
                return GL_UNSIGNED_INT;
            case tbx::GraphicsVertexFormat::INT32:
                return GL_INT;
            case tbx::GraphicsVertexFormat::FLOAT:
            case tbx::GraphicsVertexFormat::VEC2:
            case tbx::GraphicsVertexFormat::VEC3:
            case tbx::GraphicsVertexFormat::VEC4:
            default:
                return GL_FLOAT;
        }
    }

    bool is_same_buffer_slot_binding(
        const OpenGlBufferSlotBinding& binding,
        const tbx::Uuid& resource,
        const uint64 offset,
        const uint64 range)
    {
        return binding.resource == resource && binding.offset == offset && binding.range == range;
    }

    tbx::Result configure_vertex_array(
        const GLuint vertex_array,
        const tbx::RasterPipelineDesc& desc,
        std::vector<OpenGlVertexBufferBinding>& out_vertex_buffers)
    {
        out_vertex_buffers.clear();
        out_vertex_buffers.reserve(desc.vertex_buffers.size());
        for (const auto& layout : desc.vertex_buffers)
        {
            glVertexArrayBindingDivisor(
                vertex_array,
                layout.slot,
                layout.is_per_instance ? 1U : 0U);
            out_vertex_buffers.push_back(
                OpenGlVertexBufferBinding {
                    .slot = layout.slot,
                    .stride = static_cast<GLsizei>(layout.stride),
                });
        }

        for (const auto& attribute : desc.vertex_attributes)
        {
            const auto* layout = find_vertex_buffer_layout(desc, attribute.buffer_slot);
            if (layout == nullptr)
            {
                return make_failure(
                    "OpenGL backend: vertex attribute references a missing vertex buffer slot.");
            }

            glEnableVertexArrayAttrib(vertex_array, attribute.location);
            glVertexArrayAttribBinding(vertex_array, attribute.location, attribute.buffer_slot);

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
        }

        return make_success();
    }

    const OpenGlVertexBufferBinding* find_vertex_buffer_binding(
        const OpenGlRasterPipelineResource& pipeline,
        const uint32 slot)
    {
        const auto it = std::ranges::find_if(
            pipeline.vertex_buffers,
            [slot](const OpenGlVertexBufferBinding& binding)
            {
                return binding.slot == slot;
            });
        return it == pipeline.vertex_buffers.end() ? nullptr : &(*it);
    }

    const OpenGlBindGroupLayoutEntry* find_bind_group_layout_entry(
        const std::vector<OpenGlBindGroupLayoutEntry>& layout,
        const uint32 slot)
    {
        const auto it = std::ranges::find_if(
            layout,
            [slot](const OpenGlBindGroupLayoutEntry& entry)
            {
                return entry.slot == slot;
            });
        return it == layout.end() ? nullptr : &(*it);
    }

    bool can_bind_buffer_as(const OpenGlBufferResource& buffer, const OpenGlBindEntryType type)
    {
        switch (type)
        {
            case OpenGlBindEntryType::VERTEX_BUFFER:
                return buffer.is_vertex_buffer;
            case OpenGlBindEntryType::INDEX_BUFFER:
                return buffer.is_index_buffer;
            case OpenGlBindEntryType::UNIFORM_BUFFER:
                return buffer.is_uniform_buffer;
            case OpenGlBindEntryType::STORAGE_BUFFER:
                return buffer.is_storage_buffer;
            default:
                return false;
        }
    }

    tbx::Result require_buffer_capability(
        const OpenGlBufferResource& buffer,
        const OpenGlBindEntryType type,
        std::string failure_message)
    {
        if (can_bind_buffer_as(buffer, type))
            return make_success();

        return make_failure(std::move(failure_message));
    }

    OpenGlPipelineState make_pipeline_state(
        const tbx::Uuid& pipeline_resource_uuid,
        const tbx::RasterPipelineDesc& desc)
    {
        return OpenGlPipelineState {
            .id = pipeline_resource_uuid,
            .is_depth_test_enabled = desc.is_depth_test_enabled,
            .is_depth_write_enabled = desc.is_depth_write_enabled,
            .is_blending_enabled = desc.is_blending_enabled,
            .is_culling_enabled = desc.is_culling_enabled,
            .cull_mode = desc.cull_mode,
        };
    }

    void bind_buffer_slot(
        const GLenum target,
        const uint32 slot,
        const OpenGlGraphicsBuffer& buffer,
        const uint64 offset,
        const uint64 range)
    {
        if (range > 0U)
        {
            glBindBufferRange(
                target,
                slot,
                buffer.get_buffer_id(),
                static_cast<GLintptr>(offset),
                static_cast<GLsizeiptr>(range));
            return;
        }

        glBindBufferBase(target, slot, buffer.get_buffer_id());
    }

    tbx::Result require_supported_opengl_direct_state_access()
    {
        if (GLAD_GL_VERSION_4_5 && glCreateBuffers && glNamedBufferData && glNamedBufferSubData
            && glCreateVertexArrays && glVertexArrayVertexBuffer && glVertexArrayElementBuffer
            && glCreateFramebuffers && glNamedFramebufferTexture && glNamedFramebufferDrawBuffers
            && glCreateTextures)
            return make_success();

        auto message = std::string("OpenGL backend requires OpenGL ");
        message += std::to_string(OPENGL_MAJOR_VERSION);
        message += ".";
        message += std::to_string(OPENGL_MINOR_VERSION);
        message += " direct state access. ";
        message += "Driver reported version '";
        message += get_gl_string(GL_VERSION);
        message += "', renderer '";
        message += get_gl_string(GL_RENDERER);
        message += "'.";
        return make_failure(std::move(message));
    }

    OpenGlGraphicsBackend::OpenGlGraphicsBackend(
        std::weak_ptr<tbx::IOpenGlContextBackend> context_backend)
        : _context_backend(context_backend)
    {
    }

    OpenGlGraphicsBackend::~OpenGlGraphicsBackend() noexcept
    {
        try
        {
            cleanup();
        }
        catch (...)
        {
            TBX_TRACE_ERROR("OpenGL backend cleanup failed during destruction.");
        }
    }

    void OpenGlGraphicsBackend::cleanup()
    {
        const auto context_backend = lock_context_backend();

        if (_state.is_loaded && context_backend)
            destroy_resources();

        while (context_backend && !_contexts.empty())
        {
            const auto window = _contexts.back();
            context_backend->destroy_context(window);
            _contexts.pop_back();
        }
        _contexts.clear();

        _state.current_target = {};
        _state.is_loaded = false;
        _state.is_compute_pass_active = false;
        _state.is_render_pass_active = false;
    }

    tbx::GraphicsApi OpenGlGraphicsBackend::get_api() const
    {
        return tbx::GraphicsApi::OPEN_GL;
    }

    tbx::VsyncMode OpenGlGraphicsBackend::get_vsync() const
    {
        return _state.vsync_mode;
    }

    tbx::Result OpenGlGraphicsBackend::set_vsync(const tbx::VsyncMode mode)
    {
        const auto context_backend = lock_context_backend();
        if (!context_backend)
            return make_failure("OpenGL backend: context backend service is unavailable.");

        auto result = context_backend->set_vsync(mode);
        if (result)
            _state.vsync_mode = mode;

        return result;
    }

    tbx::Result OpenGlGraphicsBackend::begin_frame(const tbx::Window& output_target)
    {
        if (!output_target.id.is_valid())
            return make_failure("OpenGL backend: frame output window is invalid.");

        if (auto result = ensure_frame_context(output_target); !result)
            return result;

        clear_bound_state();
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::end_frame()
    {
        clear_bound_state();
        auto result = consume_gl_errors("end_frame");
        _state.current_target = {};
        return result;
    }

    tbx::Result OpenGlGraphicsBackend::present()
    {
        if (!_state.current_target.id.is_valid())
            return make_failure("OpenGL backend: no active window to present.");

        if (!std::ranges::contains(_contexts, _state.current_target))
            return make_failure("OpenGL backend: active window context was not found.");

        return present(_state.current_target);
    }

    void OpenGlGraphicsBackend::wait_for_idle()
    {
        if (!_state.is_loaded)
            return;

        glFinish();
    }

    tbx::Result OpenGlGraphicsBackend::begin_render_pass(const tbx::GraphicsRenderPassDesc& pass)
    {
        if (_state.is_render_pass_active)
            return make_failure("OpenGL backend: a render pass is already active.");

        _cache.pass_framebuffer.reset();
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        auto viewport = pass.viewport;
        if (!viewport.is_zero())
        {
            _state.current_viewport = viewport;
            glViewport(
                static_cast<GLint>(viewport.position.x),
                static_cast<GLint>(viewport.position.y),
                static_cast<GLsizei>(viewport.dimensions.width),
                static_cast<GLsizei>(viewport.dimensions.height));
        }

        if (!pass.color_targets.empty() || pass.depth_stencil_target.is_valid())
        {
            _cache.pass_framebuffer = std::make_unique<OpenGlFramebuffer>();

            for (uint32 index = 0U; index < pass.color_targets.size(); ++index)
            {
                const auto texture_it = _cache.textures.find(pass.color_targets[index]);
                if (texture_it == _cache.textures.end())
                    return make_failure("OpenGL backend: render pass color target was not found.");

                _cache.pass_framebuffer->attach_color(index, texture_it->second.texture);
            }

            if (pass.depth_stencil_target.is_valid())
            {
                const auto texture_it = _cache.textures.find(pass.depth_stencil_target);
                if (texture_it == _cache.textures.end())
                    return make_failure("OpenGL backend: render pass depth target was not found.");

                const bool has_layered_depth_texture = texture_it->second.array_layer_count > 1U;
                const int32 attachment_layer =
                    has_layered_depth_texture ? pass.depth_stencil_layer : -1;
                _cache.pass_framebuffer->attach_depth_stencil(
                    texture_it->second.texture,
                    texture_it->second.depth_attachment,
                    attachment_layer);
            }

            _cache.pass_framebuffer->set_draw_buffers(
                static_cast<uint32>(pass.color_targets.size()));

            if (!_cache.pass_framebuffer->is_complete())
                return make_failure("OpenGL backend: render pass framebuffer is incomplete.");

            _cache.pass_framebuffer->bind();

            auto target_size = tbx::Size {};
            if (!pass.color_targets.empty())
            {
                const auto texture_it = _cache.textures.find(pass.color_targets.front());
                if (texture_it != _cache.textures.end())
                    target_size = texture_it->second.size;
            }
            else if (pass.depth_stencil_target.is_valid())
            {
                const auto texture_it = _cache.textures.find(pass.depth_stencil_target);
                if (texture_it != _cache.textures.end())
                    target_size = texture_it->second.size;
            }

            if (target_size.width > 0U && target_size.height > 0U)
            {
                glViewport(
                    0,
                    0,
                    static_cast<GLsizei>(target_size.width),
                    static_cast<GLsizei>(target_size.height));
            }
        }
        else
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0U);
            glViewport(
                static_cast<GLint>(_state.current_viewport.position.x),
                static_cast<GLint>(_state.current_viewport.position.y),
                static_cast<GLsizei>(_state.current_viewport.dimensions.width),
                static_cast<GLsizei>(_state.current_viewport.dimensions.height));
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
            // Clear happens before pipeline state is applied for this pass; force depth writes on
            // so stale GL state from a previous pass cannot block the depth clear.
            glDepthMask(GL_TRUE);
            _state.has_current_pipeline_state = false;
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

        glColorMask(
            pass.is_color_write_enabled ? GL_TRUE : GL_FALSE,
            pass.is_color_write_enabled ? GL_TRUE : GL_FALSE,
            pass.is_color_write_enabled ? GL_TRUE : GL_FALSE,
            pass.is_color_write_enabled ? GL_TRUE : GL_FALSE);

        _state.is_render_pass_active = true;
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::end_render_pass()
    {
        if (!_state.is_render_pass_active)
            return make_failure("OpenGL backend: no render pass is active.");

        _state.is_render_pass_active = false;
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0U);
        _cache.pass_framebuffer.reset();
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::bind_raster_pipeline(const tbx::Uuid& pipeline_resource_uuid)
    {
        const auto pipeline_it = _cache.raster_pipelines.find(pipeline_resource_uuid);
        if (pipeline_it == _cache.raster_pipelines.end())
            return make_failure("OpenGL backend: raster pipeline state was not found.");
        auto& pipeline = pipeline_it->second;

        if (_state.current_pipeline_state.id == pipeline_resource_uuid)
        {
            apply_raster_pipeline_state(pipeline.state);
            return make_success();
        }

        pipeline.program.bind();
        glBindVertexArray(pipeline.vertex_array);
        apply_raster_pipeline_state(pipeline.state);

        _state.bound_vertex_buffers.clear();
        _state.bound_index_buffer = {};
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::bind_group(
        const uint32 set_index,
        const tbx::Uuid& group_resource_uuid)
    {
        // The current renderer flattens vertex, index, uniform, texture, and sampler resources
        // into one OpenGL binding group. The set index is reserved for future shader-reflection
        // layouts; individual bindings already carry absolute OpenGL slots.
        (void)set_index;

        const auto group_it = _cache.bind_groups.find(group_resource_uuid);
        if (group_it == _cache.bind_groups.end())
            return make_failure("OpenGL backend: bind group was not found.");

        for (const auto& binding : group_it->second)
        {
            if (!binding.resource.is_valid())
                continue;

            switch (binding.type)
            {
                case OpenGlBindEntryType::VERTEX_BUFFER:
                {
                    if (!_state.current_pipeline_state.id.is_valid()
                        || !_cache.raster_pipelines.contains(_state.current_pipeline_state.id))
                    {
                        return make_failure(
                            "OpenGL backend: no raster pipeline is currently bound.");
                    }

                    const auto buffer_it = _cache.buffers.find(binding.resource);
                    if (buffer_it == _cache.buffers.end())
                        return make_failure("OpenGL backend: vertex buffer was not found.");
                    if (auto result = require_buffer_capability(
                            buffer_it->second,
                            OpenGlBindEntryType::VERTEX_BUFFER,
                            "OpenGL backend: buffer is not a vertex buffer.");
                        !result)
                    {
                        return result;
                    }

                    if (const auto cached = _state.bound_vertex_buffers.find(binding.slot);
                        cached != _state.bound_vertex_buffers.end()
                        && cached->second == binding.resource)
                    {
                        break;
                    }

                    const auto& pipeline =
                        _cache.raster_pipelines.at(_state.current_pipeline_state.id);
                    const auto* layout = find_vertex_buffer_binding(pipeline, binding.slot);
                    if (!layout)
                    {
                        return make_failure(
                            "OpenGL backend: vertex buffer slot is not described by pipeline.");
                    }

                    glVertexArrayVertexBuffer(
                        pipeline.vertex_array,
                        binding.slot,
                        buffer_it->second.buffer.get_buffer_id(),
                        0,
                        layout->stride);
                    _state.bound_vertex_buffers[binding.slot] = binding.resource;
                    break;
                }
                case OpenGlBindEntryType::INDEX_BUFFER:
                {
                    if (!_state.current_pipeline_state.id.is_valid()
                        || !_cache.raster_pipelines.contains(_state.current_pipeline_state.id))
                    {
                        return make_failure(
                            "OpenGL backend: no raster pipeline is currently bound.");
                    }

                    const auto buffer_it = _cache.buffers.find(binding.resource);
                    if (buffer_it == _cache.buffers.end())
                        return make_failure("OpenGL backend: index buffer was not found.");
                    if (auto result = require_buffer_capability(
                            buffer_it->second,
                            OpenGlBindEntryType::INDEX_BUFFER,
                            "OpenGL backend: buffer is not an index buffer.");
                        !result)
                    {
                        return result;
                    }

                    if (_state.bound_index_buffer == binding.resource)
                    {
                        break;
                    }

                    glVertexArrayElementBuffer(
                        _cache.raster_pipelines.at(_state.current_pipeline_state.id).vertex_array,
                        buffer_it->second.buffer.get_buffer_id());
                    _state.bound_index_buffer = binding.resource;
                    break;
                }
                case OpenGlBindEntryType::UNIFORM_BUFFER:
                {
                    const auto buffer_it = _cache.buffers.find(binding.resource);
                    if (buffer_it == _cache.buffers.end())
                        return make_failure("OpenGL backend: uniform buffer was not found.");
                    if (auto result = require_buffer_capability(
                            buffer_it->second,
                            OpenGlBindEntryType::UNIFORM_BUFFER,
                            "OpenGL backend: buffer is not a uniform buffer.");
                        !result)
                    {
                        return result;
                    }

                    if (_state.max_uniform_buffer_bindings < 0)
                    {
                        GLint max_uniform_buffer_bindings = 0;
                        glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &max_uniform_buffer_bindings);
                        _state.max_uniform_buffer_bindings =
                            std::max(max_uniform_buffer_bindings, 0);
                    }
                    if (binding.slot >= static_cast<uint32>(_state.max_uniform_buffer_bindings))
                    {
                        return make_failure(
                            "OpenGL backend: uniform buffer bind failed, slot "
                            + std::to_string(binding.slot)
                            + " exceeds GL_MAX_UNIFORM_BUFFER_BINDINGS="
                            + std::to_string(_state.max_uniform_buffer_bindings) + ".");
                    }

                    if (const auto cached = _state.bound_uniform_buffers.find(binding.slot);
                        cached != _state.bound_uniform_buffers.end()
                        && is_same_buffer_slot_binding(
                            cached->second,
                            binding.resource,
                            binding.offset,
                            binding.range))
                    {
                        break;
                    }

                    bind_buffer_slot(
                        GL_UNIFORM_BUFFER,
                        binding.slot,
                        buffer_it->second.buffer,
                        binding.offset,
                        binding.range);
                    _state.bound_uniform_buffers[binding.slot] = OpenGlBufferSlotBinding {
                        .resource = binding.resource,
                        .offset = binding.offset,
                        .range = binding.range};
                    break;
                }
                case OpenGlBindEntryType::STORAGE_BUFFER:
                {
                    const auto buffer_it = _cache.buffers.find(binding.resource);
                    if (buffer_it == _cache.buffers.end())
                        return make_failure("OpenGL backend: storage buffer was not found.");
                    if (auto result = require_buffer_capability(
                            buffer_it->second,
                            OpenGlBindEntryType::STORAGE_BUFFER,
                            "OpenGL backend: buffer is not a storage buffer.");
                        !result)
                    {
                        return result;
                    }

                    if (const auto cached = _state.bound_storage_buffers.find(binding.slot);
                        cached != _state.bound_storage_buffers.end()
                        && is_same_buffer_slot_binding(
                            cached->second,
                            binding.resource,
                            binding.offset,
                            binding.range))
                    {
                        break;
                    }

                    bind_buffer_slot(
                        GL_SHADER_STORAGE_BUFFER,
                        binding.slot,
                        buffer_it->second.buffer,
                        binding.offset,
                        binding.range);
                    _state.bound_storage_buffers[binding.slot] = OpenGlBufferSlotBinding {
                        .resource = binding.resource,
                        .offset = binding.offset,
                        .range = binding.range};
                    break;
                }
                case OpenGlBindEntryType::SAMPLED_TEXTURE:
                {
                    const auto texture_it = _cache.textures.find(binding.resource);
                    if (texture_it == _cache.textures.end())
                        return make_failure("OpenGL backend: texture was not found.");

                    if (const auto cached = _state.bound_sampled_textures.find(binding.slot);
                        cached != _state.bound_sampled_textures.end()
                        && cached->second == binding.resource)
                    {
                        break;
                    }

                    texture_it->second.texture.bind_slot(binding.slot);
                    _state.bound_sampled_textures[binding.slot] = binding.resource;
                    break;
                }
                case OpenGlBindEntryType::STORAGE_TEXTURE:
                {
                    const auto texture_it = _cache.textures.find(binding.resource);
                    if (texture_it == _cache.textures.end())
                        return make_failure("OpenGL backend: storage texture was not found.");

                    if (const auto cached = _state.bound_image_textures.find(binding.slot);
                        cached != _state.bound_image_textures.end()
                        && cached->second == binding.resource)
                    {
                        break;
                    }

                    glBindImageTexture(
                        binding.slot,
                        texture_it->second.texture.get_texture_id(),
                        0,
                        texture_it->second.array_layer_count > 1U ? GL_TRUE : GL_FALSE,
                        0,
                        GL_READ_WRITE,
                        texture_it->second.internal_format);
                    _state.bound_image_textures[binding.slot] = binding.resource;
                    break;
                }
                case OpenGlBindEntryType::SAMPLER:
                {
                    const auto sampler_it = _cache.samplers.find(binding.resource);
                    if (sampler_it == _cache.samplers.end())
                        return make_failure("OpenGL backend: sampler was not found.");

                    if (const auto cached = _state.bound_samplers.find(binding.slot);
                        cached != _state.bound_samplers.end() && cached->second == binding.resource)
                    {
                        break;
                    }

                    sampler_it->second.bind_slot(binding.slot);
                    _state.bound_samplers[binding.slot] = binding.resource;
                    break;
                }
                default:
                    return make_failure(
                        "OpenGL backend: bind group entry type was not recognized.");
            }
        }

        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::bind_compute_pipeline(
        const tbx::Uuid& pipeline_resource_uuid)
    {
        if (_state.current_pipeline_state.id == pipeline_resource_uuid
            && _cache.compute_pipelines.contains(pipeline_resource_uuid))
        {
            return make_success();
        }

        const auto pipeline_it = _cache.compute_pipelines.find(pipeline_resource_uuid);
        if (pipeline_it == _cache.compute_pipelines.end())
            return make_failure("OpenGL backend: compute pipeline was not found.");

        pipeline_it->second.program.bind();
        glBindVertexArray(0U);

        _state.current_pipeline_state.id = pipeline_resource_uuid;
        _state.bound_vertex_buffers.clear();
        _state.bound_index_buffer = {};
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::draw(
        const uint32 index_count,
        const uint32 instance_count,
        const uint32 first_index,
        const int32 vertex_offset,
        const uint32 first_instance)
    {
        const auto pipeline_it = _cache.raster_pipelines.find(_state.current_pipeline_state.id);
        if (!_state.current_pipeline_state.id.is_valid()
            || pipeline_it == _cache.raster_pipelines.end())
        {
            return make_failure("OpenGL backend: no raster pipeline is currently bound.");
        }

        const auto& pipeline = pipeline_it->second;
        if (_state.bound_index_buffer.is_valid())
        {
            const uint64 index_byte_offset = static_cast<uint64>(first_index) * 4U;
            const auto* index_offset =
                reinterpret_cast<const void*>(static_cast<std::uintptr_t>(index_byte_offset));
            glDrawElementsInstancedBaseVertexBaseInstance(
                pipeline.primitive_type,
                static_cast<GLsizei>(index_count),
                GL_UNSIGNED_INT,
                index_offset,
                static_cast<GLsizei>(instance_count),
                vertex_offset,
                first_instance);
            return make_success();
        }

        glDrawArraysInstancedBaseInstance(
            pipeline.primitive_type,
            static_cast<GLint>(first_index),
            static_cast<GLsizei>(index_count),
            static_cast<GLsizei>(instance_count),
            first_instance);
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::draw_indirect(
        const tbx::Uuid& argument_buffer,
        const uint64 offset,
        const uint32 draw_count,
        const uint32 stride)
    {
        const auto pipeline_it = _cache.raster_pipelines.find(_state.current_pipeline_state.id);
        if (!_state.current_pipeline_state.id.is_valid()
            || pipeline_it == _cache.raster_pipelines.end())
        {
            return make_failure("OpenGL backend: no raster pipeline is currently bound.");
        }

        const auto buffer_it = _cache.buffers.find(argument_buffer);
        if (buffer_it == _cache.buffers.end())
            return make_failure("OpenGL backend: indirect argument buffer was not found.");
        if (!buffer_it->second.is_indirect_argument_buffer)
            return make_failure("OpenGL backend: buffer is not an indirect argument buffer.");

        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, buffer_it->second.buffer.get_buffer_id());
        const auto* indirect_offset =
            reinterpret_cast<const void*>(static_cast<std::uintptr_t>(offset));
        const auto& pipeline = pipeline_it->second;
        if (_state.bound_index_buffer.is_valid())
        {
            glMultiDrawElementsIndirect(
                pipeline.primitive_type,
                GL_UNSIGNED_INT,
                indirect_offset,
                static_cast<GLsizei>(draw_count),
                static_cast<GLsizei>(stride));
        }
        else
        {
            glMultiDrawArraysIndirect(
                pipeline.primitive_type,
                indirect_offset,
                static_cast<GLsizei>(draw_count),
                static_cast<GLsizei>(stride));
        }
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::dispatch_compute(
        const uint32 group_count_x,
        const uint32 group_count_y,
        const uint32 group_count_z)
    {
        if (!_state.current_pipeline_state.id.is_valid()
            || !_cache.compute_pipelines.contains(_state.current_pipeline_state.id))
        {
            return make_failure("OpenGL backend: no compute pipeline is currently bound.");
        }

        glDispatchCompute(group_count_x, group_count_y, group_count_z);
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::begin_compute_pass(const tbx::GraphicsComputePassDesc& pass)
    {
        (void)pass;
        if (_state.is_render_pass_active)
            return make_failure("OpenGL backend: a render pass is already active.");
        if (_state.is_compute_pass_active)
            return make_failure("OpenGL backend: a compute pass is already active.");

        _state.is_compute_pass_active = true;
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::destroy_resource(const tbx::Uuid& resource_uuid)
    {
        if (auto group_it = _cache.bind_groups.find(resource_uuid);
            group_it != _cache.bind_groups.end())
        {
            _cache.bind_groups.erase(group_it);
            return make_success();
        }

        if (auto layout_it = _cache.bind_group_layouts.find(resource_uuid);
            layout_it != _cache.bind_group_layouts.end())
        {
            _cache.bind_group_layouts.erase(layout_it);
            return make_success();
        }

        if (auto buffer_it = _cache.buffers.find(resource_uuid); buffer_it != _cache.buffers.end())
        {
            _cache.buffers.erase(buffer_it);
            clear_bound_state();
            return make_success();
        }

        if (auto pipeline_it = _cache.raster_pipelines.find(resource_uuid);
            pipeline_it != _cache.raster_pipelines.end())
        {
            if (pipeline_it->second.vertex_array != 0U)
                glDeleteVertexArrays(1, &pipeline_it->second.vertex_array);
            _cache.raster_pipelines.erase(pipeline_it);
            if (_state.current_pipeline_state.id == resource_uuid)
                clear_bound_state();
            return make_success();
        }

        if (auto pipeline_it = _cache.compute_pipelines.find(resource_uuid);
            pipeline_it != _cache.compute_pipelines.end())
        {
            _cache.compute_pipelines.erase(pipeline_it);
            if (_state.current_pipeline_state.id == resource_uuid)
                clear_bound_state();
            return make_success();
        }

        if (auto sampler_it = _cache.samplers.find(resource_uuid);
            sampler_it != _cache.samplers.end())
        {
            _cache.samplers.erase(sampler_it);
            clear_bound_state();
            return make_success();
        }

        if (auto texture_it = _cache.textures.find(resource_uuid);
            texture_it != _cache.textures.end())
        {
            _cache.textures.erase(texture_it);
            clear_bound_state();
            return make_success();
        }

        return make_failure("OpenGL backend: resource was not found.");
    }

    tbx::Result OpenGlGraphicsBackend::end_compute_pass()
    {
        if (!_state.is_compute_pass_active)
            return make_failure("OpenGL backend: no compute pass is active.");

        _state.is_compute_pass_active = false;
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::pipeline_barrier(
        const std::vector<tbx::PipelineBarrierDesc>& barriers)
    {
        auto barrier_bits = GLbitfield(0U);
        for (const auto& barrier : barriers)
        {
            switch (barrier.state_after)
            {
                case tbx::ResourceState::RENDER_TARGET:
                case tbx::ResourceState::DEPTH_WRITE:
                case tbx::ResourceState::DEPTH_READ:
                    barrier_bits |= GL_FRAMEBUFFER_BARRIER_BIT;
                    break;
                case tbx::ResourceState::SHADER_READ_ONLY:
                    barrier_bits |= GL_TEXTURE_FETCH_BARRIER_BIT;
                    break;
                case tbx::ResourceState::UNORDERED_ACCESS:
                    barrier_bits |=
                        GL_SHADER_STORAGE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT;
                    break;
                case tbx::ResourceState::INDIRECT_ARGUMENT:
                    barrier_bits |= GL_COMMAND_BARRIER_BIT;
                    break;
                case tbx::ResourceState::UNDEFINED:
                default:
                    break;
            }
        }

        if (barrier_bits != 0U)
            glMemoryBarrier(barrier_bits);

        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::create_bind_group(
        const tbx::BindGroupDesc& desc,
        tbx::Uuid& out_resource_uuid)
    {
        const auto layout_it = _cache.bind_group_layouts.find(desc.layout_handle);
        const std::vector<OpenGlBindGroupLayoutEntry>* layout =
            layout_it == _cache.bind_group_layouts.end() ? nullptr : &layout_it->second;
        auto bind_entries = std::vector<OpenGlBindEntry>();
        bind_entries.reserve(desc.bindings.size());

        for (const auto& binding : desc.bindings)
        {
            if (!binding.resource_handle.is_valid())
                continue;

            auto entry_type = OpenGlBindEntryType::UNIFORM_BUFFER;
            auto has_type = false;
            if (layout != nullptr)
            {
                if (const auto* layout_entry =
                        find_bind_group_layout_entry(*layout, binding.binding_slot))
                {
                    entry_type = layout_entry->type;
                    has_type = true;
                }
            }

            if (!has_type)
            {
                const auto buffer_it = _cache.buffers.find(binding.resource_handle);
                if (buffer_it != _cache.buffers.end())
                {
                    if (buffer_it->second.is_vertex_buffer)
                        entry_type = OpenGlBindEntryType::VERTEX_BUFFER;
                    else if (buffer_it->second.is_index_buffer)
                        entry_type = OpenGlBindEntryType::INDEX_BUFFER;
                    else if (buffer_it->second.is_uniform_buffer)
                        entry_type = OpenGlBindEntryType::UNIFORM_BUFFER;
                    else if (buffer_it->second.is_storage_buffer)
                        entry_type = OpenGlBindEntryType::STORAGE_BUFFER;
                    else
                    {
                        return make_failure(
                            "OpenGL backend: bind group buffer usage was not recognized.");
                    }
                    has_type = true;
                }
            }

            if (!has_type)
            {
                const auto texture_it = _cache.textures.find(binding.resource_handle);
                if (texture_it != _cache.textures.end())
                {
                    entry_type = texture_it->second.is_storage_capable
                                     ? OpenGlBindEntryType::STORAGE_TEXTURE
                                     : OpenGlBindEntryType::SAMPLED_TEXTURE;
                    has_type = true;
                }
            }

            if (!has_type && _cache.samplers.contains(binding.resource_handle))
            {
                entry_type = OpenGlBindEntryType::SAMPLER;
                has_type = true;
            }

            if (!has_type)
                return make_failure("OpenGL backend: bind group resource type was not recognized.");

            bind_entries.push_back(
                OpenGlBindEntry {
                    .type = entry_type,
                    .slot = binding.binding_slot,
                    .resource = binding.resource_handle,
                    .offset = binding.offset,
                    .range = binding.range,
                });
        }

        out_resource_uuid = tbx::Uuid::generate();
        _cache.bind_groups.emplace(out_resource_uuid, std::move(bind_entries));
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::create_bind_group_layout(
        const tbx::BindGroupLayoutDesc& desc,
        tbx::Uuid& out_resource_uuid)
    {
        auto layout = std::vector<OpenGlBindGroupLayoutEntry> {};
        layout.reserve(desc.entries.size());
        for (const auto& entry : desc.entries)
        {
            auto type = OpenGlBindEntryType::UNIFORM_BUFFER;
            if (entry.type == tbx::BindingType::STORAGE_BUFFER
                || entry.type == tbx::BindingType::STORAGE_BUFFER_DYNAMIC)
            {
                type = OpenGlBindEntryType::STORAGE_BUFFER;
            }
            else if (entry.type == tbx::BindingType::SAMPLED_TEXTURE)
            {
                type = OpenGlBindEntryType::SAMPLED_TEXTURE;
            }
            else if (entry.type == tbx::BindingType::STORAGE_TEXTURE)
            {
                type = OpenGlBindEntryType::STORAGE_TEXTURE;
            }

            layout.push_back(
                OpenGlBindGroupLayoutEntry {
                    .slot = entry.binding_slot,
                    .type = type,
                });
        }

        out_resource_uuid = tbx::Uuid::generate();
        _cache.bind_group_layouts.emplace(out_resource_uuid, std::move(layout));
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::create_buffer(
        const tbx::GraphicsBufferDesc& desc,
        tbx::Uuid& out_resource_uuid)
    {
        if (auto result = require_gl_ready_for_resource_ops(); !result)
            return result;

        if (desc.size == 0U)
            return make_failure("OpenGL backend: buffer size must be greater than zero.");

        out_resource_uuid = tbx::Uuid::generate();
        _cache.buffers.emplace(
            out_resource_uuid,
            OpenGlBufferResource {
                .buffer = OpenGlGraphicsBuffer(desc, nullptr, 0U),
                .size = desc.size,
                .is_vertex_buffer = has_buffer_usage(desc.usage, tbx::GraphicsBufferUsage::VERTEX),
                .is_index_buffer = has_buffer_usage(desc.usage, tbx::GraphicsBufferUsage::INDEX),
                .is_uniform_buffer =
                    has_buffer_usage(desc.usage, tbx::GraphicsBufferUsage::UNIFORM),
                .is_storage_buffer =
                    has_buffer_usage(desc.usage, tbx::GraphicsBufferUsage::STORAGE),
                .is_indirect_argument_buffer =
                    has_buffer_usage(desc.usage, tbx::GraphicsBufferUsage::INDIRECT_ARGS),
            });
        if (auto result = consume_gl_errors("create_buffer"); !result)
        {
            _cache.buffers.erase(out_resource_uuid);
            out_resource_uuid = {};
            return result;
        }

        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::create_compute_pipeline(
        const tbx::ComputePipelineDesc& desc,
        tbx::Uuid& out_resource_uuid)
    {
        if (auto result = require_gl_ready_for_resource_ops(); !result)
            return result;

        auto shaders = std::vector<std::shared_ptr<OpenGlShader>> {};
        if (auto result = create_shaders(desc.shaders, shaders); !result)
        {
            auto message = std::string("OpenGL backend: compute shader upload failed");
            if (!desc.debug_name.empty())
                message += " for pipeline '" + desc.debug_name + "'";
            message += ". ";
            message += result.get_report();
            return make_failure(std::move(message));
        }

        auto program = OpenGlShaderProgram(shaders);
        if (program.get_program_id() == 0U)
        {
            auto message = std::string("OpenGL backend: compute program link failed");
            if (!desc.debug_name.empty())
                message += " for pipeline '" + desc.debug_name + "'";
            message += ". ";
            message += program.get_last_error().empty() ? "No driver error log was provided."
                                                        : program.get_last_error();
            return make_failure(std::move(message));
        }

        out_resource_uuid = tbx::Uuid::generate();
        _cache.compute_pipelines.emplace(
            out_resource_uuid,
            OpenGlComputePipelineResource {.program = std::move(program)});
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::create_raster_pipeline(
        const tbx::RasterPipelineDesc& desc,
        tbx::Uuid& out_resource_uuid)
    {
        if (auto result = require_gl_ready_for_resource_ops(); !result)
            return result;

        auto shaders = std::vector<std::shared_ptr<OpenGlShader>> {};
        if (auto result = create_shaders(desc.shaders, shaders); !result)
        {
            auto message = std::string("OpenGL backend: shader upload failed");
            if (!desc.debug_name.empty())
                message += " for pipeline '" + desc.debug_name + "'";
            message += ". ";
            message += result.get_report();
            return make_failure(std::move(message));
        }

        auto program = OpenGlShaderProgram(shaders);
        if (program.get_program_id() == 0U)
        {
            auto message = std::string("OpenGL backend: shader program link failed");
            if (!desc.debug_name.empty())
                message += " for pipeline '" + desc.debug_name + "'";
            message += ". ";
            message += program.get_last_error().empty() ? "No driver error log was provided."
                                                        : program.get_last_error();
            return make_failure(std::move(message));
        }

        auto vertex_array = GLuint {0U};
        auto vertex_buffers = std::vector<OpenGlVertexBufferBinding> {};
        glCreateVertexArrays(1, &vertex_array);
        if (auto result = configure_vertex_array(vertex_array, desc, vertex_buffers); !result)
        {
            glDeleteVertexArrays(1, &vertex_array);
            return result;
        }
        if (auto result = consume_gl_errors("create_raster_pipeline"); !result)
        {
            glDeleteVertexArrays(1, &vertex_array);
            return result;
        }

        out_resource_uuid = tbx::Uuid::generate();
        _cache.raster_pipelines.emplace(
            out_resource_uuid,
            OpenGlRasterPipelineResource {
                .program = std::move(program),
                .vertex_array = vertex_array,
                .state = make_pipeline_state(out_resource_uuid, desc),
                .primitive_type = to_gl_primitive_type(desc.primitive_type),
                .vertex_buffers = std::move(vertex_buffers),
            });
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::create_sampler(
        const tbx::GraphicsSamplerDesc& desc,
        tbx::Uuid& out_resource_uuid)
    {
        if (auto result = require_gl_ready_for_resource_ops(); !result)
            return result;

        out_resource_uuid = tbx::Uuid::generate();
        _cache.samplers.emplace(out_resource_uuid, desc);
        if (auto result = consume_gl_errors("create_sampler"); !result)
        {
            _cache.samplers.erase(out_resource_uuid);
            out_resource_uuid = {};
            return result;
        }

        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::create_texture(
        const tbx::GraphicsTextureDesc& desc,
        tbx::Uuid& out_resource_uuid)
    {
        if (auto result = require_gl_ready_for_resource_ops(); !result)
            return result;

        if (desc.size.width == 0U || desc.size.height == 0U)
            return make_failure("OpenGL backend: texture size must be greater than zero.");

        out_resource_uuid = tbx::Uuid::generate();
        _cache.textures.emplace(
            out_resource_uuid,
            OpenGlTextureResource {
                .texture = OpenGlTexture(desc, nullptr),
                .size = desc.size,
                .bytes_per_pixel = get_texture_bytes_per_pixel(desc.format),
                .array_layer_count = std::max(desc.array_layer_count, 1U),
                .depth_attachment = get_depth_attachment(desc.format),
                .internal_format = get_texture_internal_format(desc.format),
                .upload_format = get_texture_upload_format(desc.format),
                .upload_type = get_texture_upload_type(desc.format),
                .is_storage_capable =
                    has_texture_usage(desc.usage, tbx::GraphicsTextureUsage::STORAGE),
            });
        if (auto result = consume_gl_errors("create_texture"); !result)
        {
            _cache.textures.erase(out_resource_uuid);
            out_resource_uuid = {};
            return result;
        }

        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::write_buffer(
        const tbx::Uuid& resource_uuid,
        const void* data,
        const uint64 data_size,
        const uint64 offset)
    {
        if (auto result = require_gl_ready_for_resource_ops(); !result)
            return result;

        const auto buffer_it = _cache.buffers.find(resource_uuid);
        if (buffer_it == _cache.buffers.end())
            return make_failure("OpenGL backend: buffer was not found.");

        if (data_size > 0U && !data)
            return make_failure("OpenGL backend: buffer update data is null.");

        if (offset + data_size > buffer_it->second.size)
            return make_failure("OpenGL backend: buffer update exceeds buffer size.");

        buffer_it->second.buffer.update(data, data_size, offset);
        return consume_gl_errors("update_buffer");
    }

    tbx::Result OpenGlGraphicsBackend::write_texture(
        const tbx::Uuid& resource_uuid,
        const tbx::GraphicsTextureUpdateDesc& desc,
        const void* data,
        const uint64 data_size)
    {
        if (auto result = require_gl_ready_for_resource_ops(); !result)
            return result;

        const auto texture_it = _cache.textures.find(resource_uuid);
        if (texture_it == _cache.textures.end())
            return make_failure("OpenGL backend: texture was not found.");

        if (data_size > 0U && !data)
            return make_failure("OpenGL backend: texture update data is null.");

        const auto& texture = texture_it->second;
        if (desc.x + desc.width > texture.size.width || desc.y + desc.height > texture.size.height)
            return make_failure("OpenGL backend: texture update exceeds texture bounds.");
        if (desc.array_layer >= texture.array_layer_count)
            return make_failure("OpenGL backend: texture update array layer is out of bounds.");

        const uint64 update_byte_size = static_cast<uint64>(desc.width)
                                        * static_cast<uint64>(desc.height)
                                        * texture.bytes_per_pixel;
        if (data != nullptr && data_size < update_byte_size)
            return make_failure("OpenGL backend: texture update data is smaller than region size.");

        texture.texture.update(desc, texture.upload_format, texture.upload_type, data);
        return consume_gl_errors("update_texture");
    }

    void OpenGlGraphicsBackend::destroy_context(const tbx::Window& window)
    {
        const auto context_it = std::ranges::find(_contexts, window);
        if (context_it == _contexts.end())
            return;

        if (_state.current_target == window)
        {
            _state.current_target = {};
            _state.current_pipeline_state = {};
        }

        const auto context_backend = lock_context_backend();
        if (!context_backend)
        {
            _contexts.erase(context_it);
            return;
        }

        context_backend->destroy_context(window);
        _contexts.erase(context_it);
    }

    tbx::Result OpenGlGraphicsBackend::make_current(const tbx::Window window) const
    {
        if (!window.id.is_valid())
            return make_failure("OpenGL backend: context window is invalid.");

        const auto context_backend = lock_context_backend();
        if (!context_backend)
            return make_failure("OpenGL backend: context backend service is unavailable.");

        return context_backend->make_context_current(window);
    }

    tbx::Result OpenGlGraphicsBackend::present(const tbx::Window window) const
    {
        if (!window.id.is_valid())
            return make_failure("OpenGL backend: context window is invalid.");

        const auto context_backend = lock_context_backend();
        if (!context_backend)
            return make_failure("OpenGL backend: context backend service is unavailable.");

        return context_backend->swap_buffers(window);
    }

    void OpenGlGraphicsBackend::clear_bound_state()
    {
        _state.current_pipeline_state = {};
        _state.is_compute_pass_active = false;
        _state.bound_index_buffer = {};
        _state.bound_image_textures.clear();
        _state.bound_samplers.clear();
        _state.bound_storage_buffers.clear();
        _state.bound_sampled_textures.clear();
        _state.bound_uniform_buffers.clear();
        _state.bound_vertex_buffers.clear();
        _state.has_current_pipeline_state = false;
        glUseProgram(0U);
        glBindVertexArray(0U);
        glBindBuffer(GL_ARRAY_BUFFER, 0U);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0U);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }

    void OpenGlGraphicsBackend::destroy_resources()
    {
        _cache.pass_framebuffer.reset();
        for (auto& pipeline_entry : _cache.raster_pipelines)
        {
            auto& vertex_array = pipeline_entry.second.vertex_array;
            if (vertex_array != 0U)
                glDeleteVertexArrays(1, &vertex_array);
        }
        _cache.raster_pipelines.clear();
        _cache.compute_pipelines.clear();
        _cache.bind_groups.clear();
        _cache.bind_group_layouts.clear();
        _cache.buffers.clear();
        _cache.samplers.clear();
        _cache.textures.clear();
        clear_bound_state();
    }

    void OpenGlGraphicsBackend::apply_raster_pipeline_state(const OpenGlPipelineState& state)
    {
        if (_state.has_current_pipeline_state
            && _state.current_pipeline_state.is_depth_test_enabled == state.is_depth_test_enabled
            && _state.current_pipeline_state.is_depth_write_enabled == state.is_depth_write_enabled
            && _state.current_pipeline_state.is_blending_enabled == state.is_blending_enabled
            && _state.current_pipeline_state.is_culling_enabled == state.is_culling_enabled
            && _state.current_pipeline_state.cull_mode == state.cull_mode)
        {
            _state.current_pipeline_state = state;
            return;
        }

        if (!_state.has_current_pipeline_state
            || _state.current_pipeline_state.is_depth_test_enabled != state.is_depth_test_enabled)
        {
            state.is_depth_test_enabled ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
        }
        if (!_state.has_current_pipeline_state
            || _state.current_pipeline_state.is_depth_write_enabled != state.is_depth_write_enabled)
        {
            glDepthMask(state.is_depth_write_enabled ? GL_TRUE : GL_FALSE);
        }
        if (!_state.has_current_pipeline_state
            || _state.current_pipeline_state.is_blending_enabled != state.is_blending_enabled)
        {
            state.is_blending_enabled ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
        }
        if (!_state.has_current_pipeline_state
            || _state.current_pipeline_state.is_culling_enabled != state.is_culling_enabled)
        {
            state.is_culling_enabled ? glEnable(GL_CULL_FACE) : glDisable(GL_CULL_FACE);
        }
        if (state.is_culling_enabled
            && (!_state.has_current_pipeline_state
                || _state.current_pipeline_state.cull_mode != state.cull_mode))
        {
            glCullFace(state.cull_mode == tbx::GraphicsCullMode::FRONT ? GL_FRONT : GL_BACK);
        }

        if (state.is_blending_enabled
            && (!_state.has_current_pipeline_state
                || !_state.current_pipeline_state.is_blending_enabled))
        {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }

        _state.current_pipeline_state = state;
        _state.has_current_pipeline_state = true;
    }

    tbx::Result OpenGlGraphicsBackend::ensure_frame_context(const tbx::Window& window)
    {
        const auto context_it = std::ranges::find(_contexts, window);
        if (context_it == _contexts.end())
        {
            const auto context_backend = lock_context_backend();
            if (!context_backend)
                return make_failure("OpenGL backend: context backend service is unavailable.");

            if (auto result = context_backend->create_context(window); !result)
                return result;

            _contexts.push_back(window);
            if (auto result = make_current(window); !result)
            {
                context_backend->destroy_context(window);
                _contexts.pop_back();
                return result;
            }
        }
        else if (auto result = make_current(window); !result)
        {
            return result;
        }

        _state.current_target = window;
        return ensure_gl_loaded();
    }

    tbx::Result OpenGlGraphicsBackend::ensure_gl_loaded()
    {
        if (_state.is_loaded)
            return make_success();

        const auto context_backend = lock_context_backend();
        if (!context_backend)
            return make_failure("OpenGL backend: context backend service is unavailable.");

        const auto loader = reinterpret_cast<GLADloadproc>(context_backend->get_proc_address());
        if (!loader || gladLoadGLLoader(loader) == 0)
            return make_failure("OpenGL backend: failed to load OpenGL functions.");

        if (auto result = require_supported_opengl_direct_state_access(); !result)
            return result;

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        _state.is_loaded = true;
        return make_success();
    }

    std::shared_ptr<tbx::IOpenGlContextBackend> OpenGlGraphicsBackend::lock_context_backend() const
    {
        return _context_backend.lock();
    }

    tbx::Result OpenGlGraphicsBackend::require_gl_ready_for_resource_ops() const
    {
        if (!_state.current_target.id.is_valid())
        {
            return make_failure(
                "OpenGL backend: begin_frame must be called before resource upload.");
        }
        if (!_state.is_loaded)
            return make_failure("OpenGL backend: OpenGL functions are not loaded.");

        return make_success();
    }
}
