#include "opengl_backend.h"
#include "internal/opengl_backend_internal.h"
#include "opengl_resources/opengl_utils.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/types/viewport.h"
#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>
namespace opengl_rendering
{
    static bool is_same_buffer_slot_binding(
        const OpenGlBufferSlotBinding& binding,
        const tbx::Uuid& resource,
        const uint64 offset,
        const uint64 range)
    {
        return binding.resource == resource && binding.offset == offset && binding.range == range;
    }

    static tbx::Result configure_vertex_array(
        const GLuint vertex_array,
        const tbx::RasterPipelineDesc& desc)
    {
        for (const auto& layout : desc.vertex_buffers)
            glVertexArrayBindingDivisor(
                vertex_array,
                layout.slot,
                layout.is_per_instance ? 1U : 0U);

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

    static void bind_buffer_slot(
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

    OpenGlGraphicsBackend::OpenGlGraphicsBackend(tbx::IOpenGlContextManager& context_manager)
        : _context_manager(context_manager)
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
        if (_is_gl_loaded)
            destroy_resources();

        while (!_contexts.empty())
        {
            const auto window = _contexts.begin()->first;
            _context_manager.destroy_context(window);
            _contexts.erase(window);
        }

        _active_window = {};
        _is_gl_loaded = false;
        _is_compute_pass_active = false;
        _is_pass_active = false;
    }

    tbx::GraphicsApi OpenGlGraphicsBackend::get_api() const
    {
        return tbx::GraphicsApi::OPEN_GL;
    }

    tbx::VsyncMode OpenGlGraphicsBackend::get_vsync() const
    {
        return _vsync_mode;
    }

    tbx::Result OpenGlGraphicsBackend::set_vsync(const tbx::VsyncMode mode)
    {
        auto result = _context_manager.set_vsync(mode);
        if (result)
            _vsync_mode = mode;

        return result;
    }

    tbx::Result OpenGlGraphicsBackend::begin_frame(const tbx::Window& output_target)
    {
        if (!output_target.is_valid())
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
        _active_window = {};
        return result;
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
        if (!_is_gl_loaded)
            return;

        glFinish();
    }

    tbx::Result OpenGlGraphicsBackend::begin_render_pass(const tbx::GraphicsRenderPassDesc& pass)
    {
        if (_is_pass_active)
            return make_failure("OpenGL backend: a render pass is already active.");

        _pass_framebuffer.reset();
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        auto viewport = pass.viewport;
        if (!viewport.is_zero())
        {
            _active_viewport = viewport;
            glViewport(
                static_cast<GLint>(viewport.position.x),
                static_cast<GLint>(viewport.position.y),
                static_cast<GLsizei>(viewport.dimensions.width),
                static_cast<GLsizei>(viewport.dimensions.height));
        }

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

                const bool has_layered_depth_texture =
                    texture_it->second.get_array_layer_count() > 1U;
                const int32 attachment_layer =
                    has_layered_depth_texture ? pass.depth_stencil_layer : -1;
                _pass_framebuffer->attach_depth_stencil(
                    texture_it->second,
                    desc_it->second.format,
                    attachment_layer);
            }

            _pass_framebuffer->set_draw_buffers(static_cast<uint32>(pass.color_targets.size()));

            if (!_pass_framebuffer->is_complete())
                return make_failure("OpenGL backend: render pass framebuffer is incomplete.");

            _pass_framebuffer->bind();

            auto target_size = tbx::Size {};
            if (!pass.color_targets.empty())
            {
                const auto desc_it = _texture_descs.find(pass.color_targets.front());
                if (desc_it != _texture_descs.end())
                    target_size = desc_it->second.size;
            }
            else if (pass.depth_stencil_target.is_valid())
            {
                const auto desc_it = _texture_descs.find(pass.depth_stencil_target);
                if (desc_it != _texture_descs.end())
                    target_size = desc_it->second.size;
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
                static_cast<GLint>(_active_viewport.position.x),
                static_cast<GLint>(_active_viewport.position.y),
                static_cast<GLsizei>(_active_viewport.dimensions.width),
                static_cast<GLsizei>(_active_viewport.dimensions.height));
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
            _has_current_pipeline_state = false;
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

        _is_pass_active = true;
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::end_render_pass()
    {
        if (!_is_pass_active)
            return make_failure("OpenGL backend: no render pass is active.");

        _is_pass_active = false;
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0U);
        _pass_framebuffer.reset();
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::bind_raster_pipeline(const tbx::Uuid& pipeline_resource_uuid)
    {
        const auto program_it = _programs.find(pipeline_resource_uuid);
        if (program_it == _programs.end())
            return make_failure("OpenGL backend: raster pipeline was not found.");

        const auto vertex_array_it = _pipeline_vertex_arrays.find(pipeline_resource_uuid);
        const auto desc_it = _raster_pipeline_descs.find(pipeline_resource_uuid);
        if (vertex_array_it == _pipeline_vertex_arrays.end()
            || desc_it == _raster_pipeline_descs.end())
        {
            return make_failure("OpenGL backend: raster pipeline state was not found.");
        }

        if (_current_pipeline == pipeline_resource_uuid)
        {
            apply_raster_pipeline_state(desc_it->second);
            return make_success();
        }

        program_it->second.bind();
        glBindVertexArray(vertex_array_it->second);
        apply_raster_pipeline_state(desc_it->second);

        _current_pipeline = pipeline_resource_uuid;
        _current_index_type = tbx::GraphicsIndexType::UINT32;
        _bound_vertex_buffers.clear();
        _bound_index_buffer = {};
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

        const auto group_it = _bind_groups.find(group_resource_uuid);
        if (group_it == _bind_groups.end())
            return make_failure("OpenGL backend: bind group was not found.");

        for (const auto& binding : group_it->second)
        {
            if (!binding.resource.is_valid())
                continue;

            switch (binding.type)
            {
                case OpenGlBindEntryType::VERTEX_BUFFER:
                {
                    if (!_current_pipeline.is_valid()
                        || !_raster_pipeline_descs.contains(_current_pipeline)
                        || !_pipeline_vertex_arrays.contains(_current_pipeline))
                    {
                        return make_failure(
                            "OpenGL backend: no raster pipeline is currently bound.");
                    }

                    const auto buffer_it = _buffers.find(binding.resource);
                    if (buffer_it == _buffers.end())
                        return make_failure("OpenGL backend: vertex buffer was not found.");
                    const auto buffer_desc_it = _buffer_descs.find(binding.resource);
                    if (buffer_desc_it == _buffer_descs.end())
                        return make_failure(
                            "OpenGL backend: vertex buffer description was not found.");
                    if (auto result = require_buffer_usage(
                            buffer_desc_it->second,
                            tbx::GraphicsBufferUsage::VERTEX,
                            "OpenGL backend: buffer is not a vertex buffer.");
                        !result)
                    {
                        return result;
                    }

                    if (const auto cached = _bound_vertex_buffers.find(binding.slot);
                        cached != _bound_vertex_buffers.end() && cached->second == binding.resource)
                    {
                        break;
                    }

                    const auto& pipeline_desc = _raster_pipeline_descs.at(_current_pipeline);
                    const auto* layout = find_vertex_buffer_layout(pipeline_desc, binding.slot);
                    if (!layout)
                    {
                        return make_failure(
                            "OpenGL backend: vertex buffer slot is not described by pipeline.");
                    }

                    glVertexArrayVertexBuffer(
                        _pipeline_vertex_arrays.at(_current_pipeline),
                        binding.slot,
                        buffer_it->second.get_buffer_id(),
                        0,
                        static_cast<GLsizei>(layout->stride));
                    _bound_vertex_buffers[binding.slot] = binding.resource;
                    break;
                }
                case OpenGlBindEntryType::INDEX_BUFFER:
                {
                    if (!_current_pipeline.is_valid()
                        || !_raster_pipeline_descs.contains(_current_pipeline)
                        || !_pipeline_vertex_arrays.contains(_current_pipeline))
                    {
                        return make_failure(
                            "OpenGL backend: no raster pipeline is currently bound.");
                    }

                    const auto buffer_it = _buffers.find(binding.resource);
                    if (buffer_it == _buffers.end())
                        return make_failure("OpenGL backend: index buffer was not found.");
                    const auto buffer_desc_it = _buffer_descs.find(binding.resource);
                    if (buffer_desc_it == _buffer_descs.end())
                        return make_failure(
                            "OpenGL backend: index buffer description was not found.");
                    if (auto result = require_buffer_usage(
                            buffer_desc_it->second,
                            tbx::GraphicsBufferUsage::INDEX,
                            "OpenGL backend: buffer is not an index buffer.");
                        !result)
                    {
                        return result;
                    }

                    const auto index_type = tbx::GraphicsIndexType::UINT32;
                    if (_bound_index_buffer == binding.resource
                        && _current_index_type == index_type)
                    {
                        break;
                    }

                    glVertexArrayElementBuffer(
                        _pipeline_vertex_arrays.at(_current_pipeline),
                        buffer_it->second.get_buffer_id());
                    _current_index_type = index_type;
                    _bound_index_buffer = binding.resource;
                    break;
                }
                case OpenGlBindEntryType::UNIFORM_BUFFER:
                {
                    const auto buffer_it = _buffers.find(binding.resource);
                    if (buffer_it == _buffers.end())
                        return make_failure("OpenGL backend: uniform buffer was not found.");
                    const auto buffer_desc_it = _buffer_descs.find(binding.resource);
                    if (buffer_desc_it == _buffer_descs.end())
                        return make_failure(
                            "OpenGL backend: uniform buffer description was not found.");
                    if (auto result = require_buffer_usage(
                            buffer_desc_it->second,
                            tbx::GraphicsBufferUsage::UNIFORM,
                            "OpenGL backend: buffer is not a uniform buffer.");
                        !result)
                    {
                        return result;
                    }

                    if (_max_uniform_buffer_bindings < 0)
                    {
                        GLint max_uniform_buffer_bindings = 0;
                        glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &max_uniform_buffer_bindings);
                        _max_uniform_buffer_bindings = std::max(max_uniform_buffer_bindings, 0);
                    }
                    if (binding.slot >= static_cast<uint32>(_max_uniform_buffer_bindings))
                    {
                        return make_failure(
                            "OpenGL backend: uniform buffer bind failed, slot "
                            + std::to_string(binding.slot)
                            + " exceeds GL_MAX_UNIFORM_BUFFER_BINDINGS="
                            + std::to_string(_max_uniform_buffer_bindings) + ".");
                    }

                    if (const auto cached = _bound_uniform_buffers.find(binding.slot);
                        cached != _bound_uniform_buffers.end()
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
                        buffer_it->second,
                        binding.offset,
                        binding.range);
                    _bound_uniform_buffers[binding.slot] = OpenGlBufferSlotBinding {
                        .resource = binding.resource,
                        .offset = binding.offset,
                        .range = binding.range};
                    break;
                }
                case OpenGlBindEntryType::STORAGE_BUFFER:
                {
                    const auto buffer_it = _buffers.find(binding.resource);
                    if (buffer_it == _buffers.end())
                        return make_failure("OpenGL backend: storage buffer was not found.");
                    const auto buffer_desc_it = _buffer_descs.find(binding.resource);
                    if (buffer_desc_it == _buffer_descs.end())
                        return make_failure(
                            "OpenGL backend: storage buffer description was not found.");
                    if (auto result = require_buffer_usage(
                            buffer_desc_it->second,
                            tbx::GraphicsBufferUsage::STORAGE,
                            "OpenGL backend: buffer is not a storage buffer.");
                        !result)
                    {
                        return result;
                    }

                    if (const auto cached = _bound_storage_buffers.find(binding.slot);
                        cached != _bound_storage_buffers.end()
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
                        buffer_it->second,
                        binding.offset,
                        binding.range);
                    _bound_storage_buffers[binding.slot] = OpenGlBufferSlotBinding {
                        .resource = binding.resource,
                        .offset = binding.offset,
                        .range = binding.range};
                    break;
                }
                case OpenGlBindEntryType::SAMPLED_TEXTURE:
                {
                    const auto texture_it = _textures.find(binding.resource);
                    if (texture_it == _textures.end())
                        return make_failure("OpenGL backend: texture was not found.");

                    if (const auto cached = _bound_textures.find(binding.slot);
                        cached != _bound_textures.end() && cached->second == binding.resource)
                    {
                        break;
                    }

                    texture_it->second.bind_slot(binding.slot);
                    _bound_textures[binding.slot] = binding.resource;
                    break;
                }
                case OpenGlBindEntryType::STORAGE_TEXTURE:
                {
                    const auto texture_it = _textures.find(binding.resource);
                    if (texture_it == _textures.end())
                        return make_failure("OpenGL backend: storage texture was not found.");
                    const auto texture_desc_it = _texture_descs.find(binding.resource);
                    if (texture_desc_it == _texture_descs.end())
                    {
                        return make_failure(
                            "OpenGL backend: storage texture description was not found.");
                    }

                    if (const auto cached = _bound_image_textures.find(binding.slot);
                        cached != _bound_image_textures.end() && cached->second == binding.resource)
                    {
                        break;
                    }

                    glBindImageTexture(
                        binding.slot,
                        texture_it->second.get_texture_id(),
                        0,
                        texture_desc_it->second.array_layer_count > 1U ? GL_TRUE : GL_FALSE,
                        0,
                        GL_READ_WRITE,
                        get_texture_internal_format(texture_desc_it->second.format));
                    _bound_image_textures[binding.slot] = binding.resource;
                    break;
                }
                case OpenGlBindEntryType::SAMPLER:
                {
                    const auto sampler_it = _samplers.find(binding.resource);
                    if (sampler_it == _samplers.end())
                        return make_failure("OpenGL backend: sampler was not found.");

                    if (const auto cached = _bound_samplers.find(binding.slot);
                        cached != _bound_samplers.end() && cached->second == binding.resource)
                    {
                        break;
                    }

                    sampler_it->second.bind_slot(binding.slot);
                    _bound_samplers[binding.slot] = binding.resource;
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
        if (_current_pipeline == pipeline_resource_uuid
            && _compute_pipeline_descs.contains(pipeline_resource_uuid))
        {
            return make_success();
        }

        const auto program_it = _programs.find(pipeline_resource_uuid);
        if (program_it == _programs.end()
            || !_compute_pipeline_descs.contains(pipeline_resource_uuid))
        {
            return make_failure("OpenGL backend: compute pipeline was not found.");
        }

        program_it->second.bind();
        glBindVertexArray(0U);

        _current_pipeline = pipeline_resource_uuid;
        _current_index_type = tbx::GraphicsIndexType::UINT32;
        _bound_vertex_buffers.clear();
        _bound_index_buffer = {};
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::draw(
        const uint32 index_count,
        const uint32 instance_count,
        const uint32 first_index,
        const int32 vertex_offset,
        const uint32 first_instance)
    {
        if (!_current_pipeline.is_valid() || !_programs.contains(_current_pipeline)
            || !_raster_pipeline_descs.contains(_current_pipeline)
            || !_pipeline_vertex_arrays.contains(_current_pipeline))
        {
            return make_failure("OpenGL backend: no raster pipeline is currently bound.");
        }

        const auto& pipeline_desc = _raster_pipeline_descs.at(_current_pipeline);
        if (_bound_index_buffer.is_valid())
        {
            const uint64 index_byte_offset =
                static_cast<uint64>(first_index)
                * (_current_index_type == tbx::GraphicsIndexType::UINT16 ? 2U : 4U);
            const auto* index_offset =
                reinterpret_cast<const void*>(static_cast<std::uintptr_t>(index_byte_offset));
            glDrawElementsInstancedBaseVertexBaseInstance(
                to_gl_primitive_type(pipeline_desc.primitive_type),
                static_cast<GLsizei>(index_count),
                to_gl_index_type(_current_index_type),
                index_offset,
                static_cast<GLsizei>(instance_count),
                vertex_offset,
                first_instance);
            return make_success();
        }

        glDrawArraysInstancedBaseInstance(
            to_gl_primitive_type(pipeline_desc.primitive_type),
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
        if (!_current_pipeline.is_valid() || !_programs.contains(_current_pipeline)
            || !_raster_pipeline_descs.contains(_current_pipeline)
            || !_pipeline_vertex_arrays.contains(_current_pipeline))
        {
            return make_failure("OpenGL backend: no raster pipeline is currently bound.");
        }

        const auto buffer_it = _buffers.find(argument_buffer);
        if (buffer_it == _buffers.end())
            return make_failure("OpenGL backend: indirect argument buffer was not found.");
        const auto buffer_desc_it = _buffer_descs.find(argument_buffer);
        if (buffer_desc_it == _buffer_descs.end())
            return make_failure(
                "OpenGL backend: indirect argument buffer description was missing.");
        if (auto result = require_buffer_usage(
                buffer_desc_it->second,
                tbx::GraphicsBufferUsage::INDIRECT_ARGS,
                "OpenGL backend: buffer is not an indirect argument buffer.");
            !result)
            return result;

        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, buffer_it->second.get_buffer_id());
        const auto* indirect_offset =
            reinterpret_cast<const void*>(static_cast<std::uintptr_t>(offset));
        const auto& pipeline_desc = _raster_pipeline_descs.at(_current_pipeline);
        if (_bound_index_buffer.is_valid())
        {
            glMultiDrawElementsIndirect(
                to_gl_primitive_type(pipeline_desc.primitive_type),
                to_gl_index_type(_current_index_type),
                indirect_offset,
                static_cast<GLsizei>(draw_count),
                static_cast<GLsizei>(stride));
        }
        else
        {
            glMultiDrawArraysIndirect(
                to_gl_primitive_type(pipeline_desc.primitive_type),
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
        if (!_current_pipeline.is_valid() || !_programs.contains(_current_pipeline)
            || !_compute_pipeline_descs.contains(_current_pipeline))
        {
            return make_failure("OpenGL backend: no compute pipeline is currently bound.");
        }

        glDispatchCompute(group_count_x, group_count_y, group_count_z);
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::begin_compute_pass(const tbx::GraphicsComputePassDesc& pass)
    {
        (void)pass;
        if (_is_pass_active)
            return make_failure("OpenGL backend: a render pass is already active.");
        if (_is_compute_pass_active)
            return make_failure("OpenGL backend: a compute pass is already active.");

        _is_compute_pass_active = true;
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::destroy_resource(const tbx::Uuid& resource_uuid)
    {
        if (auto group_it = _bind_groups.find(resource_uuid); group_it != _bind_groups.end())
        {
            _bind_groups.erase(group_it);
            return make_success();
        }

        if (auto layout_it = _bind_group_layouts.find(resource_uuid);
            layout_it != _bind_group_layouts.end())
        {
            _bind_group_layouts.erase(layout_it);
            return make_success();
        }

        if (auto buffer_it = _buffers.find(resource_uuid); buffer_it != _buffers.end())
        {
            _buffers.erase(buffer_it);
            _buffer_descs.erase(resource_uuid);
            clear_bound_state();
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
            _compute_pipeline_descs.erase(resource_uuid);
            _raster_pipeline_descs.erase(resource_uuid);
            _programs.erase(program_it);
            if (_current_pipeline == resource_uuid)
                clear_bound_state();
            return make_success();
        }

        if (auto sampler_it = _samplers.find(resource_uuid); sampler_it != _samplers.end())
        {
            _samplers.erase(sampler_it);
            clear_bound_state();
            return make_success();
        }

        if (auto texture_it = _textures.find(resource_uuid); texture_it != _textures.end())
        {
            _textures.erase(texture_it);
            _texture_descs.erase(resource_uuid);
            clear_bound_state();
            return make_success();
        }

        return make_failure("OpenGL backend: resource was not found.");
    }

    tbx::Result OpenGlGraphicsBackend::end_compute_pass()
    {
        if (!_is_compute_pass_active)
            return make_failure("OpenGL backend: no compute pass is active.");

        _is_compute_pass_active = false;
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
        const auto layout_it = _bind_group_layouts.find(desc.layout_handle);
        const tbx::BindGroupLayoutDesc* layout =
            layout_it == _bind_group_layouts.end() ? nullptr : &layout_it->second;
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
                for (const auto& layout_entry : layout->entries)
                {
                    if (layout_entry.binding_slot != binding.binding_slot)
                        continue;

                    if (layout_entry.type == tbx::BindingType::UNIFORM_BUFFER)
                        entry_type = OpenGlBindEntryType::UNIFORM_BUFFER;
                    else if (
                        layout_entry.type == tbx::BindingType::STORAGE_BUFFER
                        || layout_entry.type == tbx::BindingType::STORAGE_BUFFER_DYNAMIC)
                    {
                        entry_type = OpenGlBindEntryType::STORAGE_BUFFER;
                    }
                    else if (layout_entry.type == tbx::BindingType::SAMPLED_TEXTURE)
                        entry_type = OpenGlBindEntryType::SAMPLED_TEXTURE;
                    else if (layout_entry.type == tbx::BindingType::STORAGE_TEXTURE)
                        entry_type = OpenGlBindEntryType::STORAGE_TEXTURE;

                    has_type = true;
                    break;
                }
            }

            if (!has_type)
            {
                const auto buffer_desc_it = _buffer_descs.find(binding.resource_handle);
                if (buffer_desc_it != _buffer_descs.end())
                {
                    if (has_buffer_usage(
                            buffer_desc_it->second.usage,
                            tbx::GraphicsBufferUsage::VERTEX))
                    {
                        entry_type = OpenGlBindEntryType::VERTEX_BUFFER;
                    }
                    else if (
                        has_buffer_usage(
                            buffer_desc_it->second.usage,
                            tbx::GraphicsBufferUsage::INDEX))
                    {
                        entry_type = OpenGlBindEntryType::INDEX_BUFFER;
                    }
                    else if (
                        has_buffer_usage(
                            buffer_desc_it->second.usage,
                            tbx::GraphicsBufferUsage::UNIFORM))
                    {
                        entry_type = OpenGlBindEntryType::UNIFORM_BUFFER;
                    }
                    else if (
                        has_buffer_usage(
                            buffer_desc_it->second.usage,
                            tbx::GraphicsBufferUsage::STORAGE))
                    {
                        entry_type = OpenGlBindEntryType::STORAGE_BUFFER;
                    }
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
                const auto texture_desc_it = _texture_descs.find(binding.resource_handle);
                if (texture_desc_it != _texture_descs.end())
                {
                    entry_type = has_texture_usage(
                                     texture_desc_it->second.usage,
                                     tbx::GraphicsTextureUsage::STORAGE)
                                     ? OpenGlBindEntryType::STORAGE_TEXTURE
                                     : OpenGlBindEntryType::SAMPLED_TEXTURE;
                    has_type = true;
                }
            }

            if (!has_type && _samplers.contains(binding.resource_handle))
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
        _bind_groups.emplace(out_resource_uuid, std::move(bind_entries));
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::create_bind_group_layout(
        const tbx::BindGroupLayoutDesc& desc,
        tbx::Uuid& out_resource_uuid)
    {
        out_resource_uuid = tbx::Uuid::generate();
        _bind_group_layouts.emplace(out_resource_uuid, desc);
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
        _buffers.try_emplace(out_resource_uuid, desc, nullptr, 0U);
        _buffer_descs.emplace(out_resource_uuid, desc);
        if (auto result = consume_gl_errors("create_buffer"); !result)
        {
            _buffers.erase(out_resource_uuid);
            _buffer_descs.erase(out_resource_uuid);
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
        if (auto result = create_shaders(desc.compute_shader, shaders); !result)
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
        _programs.emplace(out_resource_uuid, std::move(program));
        _compute_pipeline_descs.emplace(out_resource_uuid, desc);
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::create_raster_pipeline(
        const tbx::RasterPipelineDesc& desc,
        tbx::Uuid& out_resource_uuid)
    {
        if (auto result = require_gl_ready_for_resource_ops(); !result)
            return result;

        auto shaders = std::vector<std::shared_ptr<OpenGlShader>> {};
        if (auto result = create_shaders(desc.shader, shaders); !result)
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
        glCreateVertexArrays(1, &vertex_array);
        if (auto result = configure_vertex_array(vertex_array, desc); !result)
        {
            glDeleteVertexArrays(1, &vertex_array);
            return result;
        }
        if (auto result = consume_gl_errors("create_raster_pipeline"); !result)
            return result;

        out_resource_uuid = tbx::Uuid::generate();
        _programs.emplace(out_resource_uuid, std::move(program));
        _raster_pipeline_descs.emplace(out_resource_uuid, desc);
        _pipeline_vertex_arrays.emplace(out_resource_uuid, vertex_array);
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::create_sampler(
        const tbx::GraphicsSamplerDesc& desc,
        tbx::Uuid& out_resource_uuid)
    {
        if (auto result = require_gl_ready_for_resource_ops(); !result)
            return result;

        out_resource_uuid = tbx::Uuid::generate();
        _samplers.emplace(out_resource_uuid, desc);
        if (auto result = consume_gl_errors("create_sampler"); !result)
        {
            _samplers.erase(out_resource_uuid);
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
        _textures.try_emplace(out_resource_uuid, desc, nullptr);
        _texture_descs.emplace(out_resource_uuid, desc);
        if (auto result = consume_gl_errors("create_texture"); !result)
        {
            _textures.erase(out_resource_uuid);
            _texture_descs.erase(out_resource_uuid);
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
        if (has_buffer_usage(buffer_desc_it->second.usage, tbx::GraphicsBufferUsage::UNIFORM))
        {
            auto operation = std::string("update_buffer(uniform='");
            operation += buffer_desc_it->second.debug_name.empty()
                             ? "unnamed"
                             : buffer_desc_it->second.debug_name;
            operation += "', bytes=";
            operation += std::to_string(data_size);
            operation += ", offset=";
            operation += std::to_string(offset);
            operation += ")";
            return consume_gl_errors(operation);
        }

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
        if (desc.array_layer >= std::max(texture_desc.array_layer_count, 1U))
            return make_failure("OpenGL backend: texture update array layer is out of bounds.");

        const uint64 update_byte_size = static_cast<uint64>(desc.width)
                                        * static_cast<uint64>(desc.height)
                                        * get_texture_bytes_per_pixel(texture_desc.format);
        if (data != nullptr && data_size < update_byte_size)
            return make_failure("OpenGL backend: texture update data is smaller than region size.");

        texture_it->second.update(desc, texture_desc.format, data);
        return consume_gl_errors("update_texture");
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
        _is_compute_pass_active = false;
        _current_index_type = tbx::GraphicsIndexType::UINT32;
        _bound_index_buffer = {};
        _bound_image_textures.clear();
        _bound_samplers.clear();
        _bound_storage_buffers.clear();
        _bound_textures.clear();
        _bound_uniform_buffers.clear();
        _bound_vertex_buffers.clear();
        _has_current_pipeline_state = false;
        glUseProgram(0U);
        glBindVertexArray(0U);
        glBindBuffer(GL_ARRAY_BUFFER, 0U);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0U);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }

    void OpenGlGraphicsBackend::destroy_resources()
    {
        _pass_framebuffer.reset();
        for (auto& pipeline_entry : _pipeline_vertex_arrays)
        {
            auto& vertex_array = pipeline_entry.second;
            if (vertex_array != 0U)
                glDeleteVertexArrays(1, &vertex_array);
        }
        _pipeline_vertex_arrays.clear();
        _compute_pipeline_descs.clear();
        _raster_pipeline_descs.clear();
        _bind_groups.clear();
        _bind_group_layouts.clear();
        _programs.clear();
        _buffers.clear();
        _buffer_descs.clear();
        _samplers.clear();
        _textures.clear();
        _texture_descs.clear();
        clear_bound_state();
    }

    void OpenGlGraphicsBackend::apply_raster_pipeline_state(const tbx::RasterPipelineDesc& desc)
    {
        if (_has_current_pipeline_state
            && _current_pipeline_state.is_depth_test_enabled == desc.is_depth_test_enabled
            && _current_pipeline_state.is_depth_write_enabled == desc.is_depth_write_enabled
            && _current_pipeline_state.is_blending_enabled == desc.is_blending_enabled
            && _current_pipeline_state.is_culling_enabled == desc.is_culling_enabled
            && _current_pipeline_state.cull_mode == desc.cull_mode)
        {
            return;
        }

        if (!_has_current_pipeline_state
            || _current_pipeline_state.is_depth_test_enabled != desc.is_depth_test_enabled)
        {
            desc.is_depth_test_enabled ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
        }
        if (!_has_current_pipeline_state
            || _current_pipeline_state.is_depth_write_enabled != desc.is_depth_write_enabled)
        {
            glDepthMask(desc.is_depth_write_enabled ? GL_TRUE : GL_FALSE);
        }
        if (!_has_current_pipeline_state
            || _current_pipeline_state.is_blending_enabled != desc.is_blending_enabled)
        {
            desc.is_blending_enabled ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
        }
        if (!_has_current_pipeline_state
            || _current_pipeline_state.is_culling_enabled != desc.is_culling_enabled)
        {
            desc.is_culling_enabled ? glEnable(GL_CULL_FACE) : glDisable(GL_CULL_FACE);
        }
        if (desc.is_culling_enabled
            && (!_has_current_pipeline_state
                || _current_pipeline_state.cull_mode != desc.cull_mode))
        {
            glCullFace(desc.cull_mode == tbx::GraphicsCullMode::FRONT ? GL_FRONT : GL_BACK);
        }

        if (desc.is_blending_enabled
            && (!_has_current_pipeline_state || !_current_pipeline_state.is_blending_enabled))
        {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }

        _current_pipeline_state = OpenGlPipelineState {
            .is_depth_test_enabled = desc.is_depth_test_enabled,
            .is_depth_write_enabled = desc.is_depth_write_enabled,
            .is_blending_enabled = desc.is_blending_enabled,
            .is_culling_enabled = desc.is_culling_enabled,
            .cull_mode = desc.cull_mode,
        };
        _has_current_pipeline_state = true;
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

        if (auto result = internal::require_opengl_4_5_direct_state_access(); !result)
            return result;

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        _is_gl_loaded = true;
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::require_gl_ready_for_resource_ops() const
    {
        if (!_active_window.is_valid())
        {
            return make_failure(
                "OpenGL backend: begin_frame must be called before resource upload.");
        }
        if (!_is_gl_loaded)
            return make_failure("OpenGL backend: OpenGL functions are not loaded.");

        return make_success();
    }
}
