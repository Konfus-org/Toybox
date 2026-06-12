#include "opengl_backend.h"
#include <chrono>
#include <thread>
#include "tbx/systems/debugging/macros.h"
#include "tbx/types/typedefs.h"
#include "tbx/types/viewport.h"

namespace opengl_rendering
{
    // Mirrors a 60Hz vsynced swap for render-texture frames.
    constexpr std::chrono::microseconds TEXTURE_FRAME_INTERVAL(16667);

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
        auto result = tbx::Result();
        result.failure(std::move(message));
        return result;
    }

    tbx::Result make_success()
    {
        auto result = tbx::Result();
        result.ok();
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

    bool has_clear_flag(const tbx::ClearFlags value, const tbx::ClearFlags flag)
    {
        return (static_cast<uint8>(value) & static_cast<uint8>(flag)) != 0U;
    }

    bool is_integer_vertex_format(const tbx::VertexFormat format)
    {
        return format == tbx::VertexFormat::UINT32
               || format == tbx::VertexFormat::INT32;
    }

    const tbx::VertexBufferLayoutDesc* find_vertex_buffer_layout(
        const tbx::RasterPipelineDesc& desc,
        const uint32 slot)
    {
        const auto it = std::ranges::find_if(
            desc.vertex_buffers,
            [slot](const tbx::VertexBufferLayoutDesc& layout)
            {
                return layout.slot == slot;
            });
        return it == desc.vertex_buffers.end() ? nullptr : &(*it);
    }

    GLenum to_gl_primitive_type(const tbx::PrimitiveType primitive_type)
    {
        switch (primitive_type)
        {
            case tbx::PrimitiveType::LINES:
                return GL_LINES;
            case tbx::PrimitiveType::POINTS:
                return GL_POINTS;
            case tbx::PrimitiveType::TRIANGLES:
            default:
                return GL_TRIANGLES;
        }
    }

    GLint get_vertex_component_count(const tbx::VertexFormat format)
    {
        switch (format)
        {
            case tbx::VertexFormat::VEC2:
                return 2;
            case tbx::VertexFormat::VEC3:
                return 3;
            case tbx::VertexFormat::VEC4:
                return 4;
            case tbx::VertexFormat::FLOAT:
            case tbx::VertexFormat::UINT32:
            case tbx::VertexFormat::INT32:
            default:
                return 1;
        }
    }

    GLenum get_vertex_component_type(const tbx::VertexFormat format)
    {
        switch (format)
        {
            case tbx::VertexFormat::UINT32:
                return GL_UNSIGNED_INT;
            case tbx::VertexFormat::INT32:
                return GL_INT;
            case tbx::VertexFormat::FLOAT:
            case tbx::VertexFormat::VEC2:
            case tbx::VertexFormat::VEC3:
            case tbx::VertexFormat::VEC4:
            default:
                return GL_FLOAT;
        }
    }

    bool is_same_buffer_slot_binding(
        const OpenGlBufferSlotBinding& binding,
        const tbx::GpuId& resource,
        const uint64 offset,
        const uint64 range)
    {
        return binding.resource == resource && binding.offset == offset && binding.range == range;
    }

    template <typename TSlotBinding>
    const TSlotBinding* find_bound_slot(
        const std::vector<TSlotBinding>& bindings,
        const uint32 slot)
    {
        const auto index = static_cast<size>(slot);
        return index < bindings.size() ? &bindings[index] : nullptr;
    }

    template <typename TSlotBinding>
    void set_bound_slot(
        std::vector<TSlotBinding>& bindings,
        const uint32 slot,
        TSlotBinding binding)
    {
        const auto index = static_cast<size>(slot);
        if (bindings.size() <= index)
            bindings.resize(index + 1U);
        bindings[index] = std::move(binding);
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
        const tbx::GpuId& pipeline_resource_uuid,
        const tbx::RasterPipelineDesc& desc)
    {
        return OpenGlPipelineState {
            .id = pipeline_resource_uuid,
            .depth_function = desc.depth_function,
            .is_depth_test_enabled = desc.is_depth_test_enabled,
            .is_depth_write_enabled = desc.is_depth_write_enabled,
            .is_blending_enabled = desc.is_blending_enabled,
            .is_culling_enabled = desc.is_culling_enabled,
            .depth_bias_constant = desc.depth_bias_constant,
            .depth_bias_slope = desc.depth_bias_slope,
            .cull_mode = desc.cull_mode,
            .blend_equation = desc.blend_equation,
        };
    }

    GLenum to_gl_depth_function(const tbx::MaterialDepthFunction function)
    {
        switch (function)
        {
            case tbx::MaterialDepthFunction::LESS:
                return GL_LESS;
            case tbx::MaterialDepthFunction::LESS_EQUAL:
                return GL_LEQUAL;
            case tbx::MaterialDepthFunction::ALWAYS:
                return GL_ALWAYS;
            default:
                return GL_LESS;
        }
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
        if (GLAD_GL_VERSION_4_6 && glCreateBuffers && glNamedBufferData && glNamedBufferSubData
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
        destroy_output_framebuffer();
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

    tbx::Result OpenGlGraphicsBackend::begin_frame(const tbx::RenderTarget& output_target)
    {
        if (!output_target.id.is_valid())
            return make_failure("OpenGL backend: frame output target is invalid.");

        // Targets arrive fully resolved from the main thread: window targets carry their native
        // handle, texture targets carry a size. No service lookups happen on the render lane.
        const auto window = tbx::Window(output_target);
        if (window.native_handle != nullptr)
        {
            if (auto result = ensure_frame_context(window); !result)
                return result;

            clear_bound_state();
            return make_success();
        }

        if (output_target.size.width == 0U || output_target.size.height == 0U)
            return make_failure(
                "OpenGL backend: frame output target has neither a native window nor a size.");

        // Texture targets render offscreen, borrowing whichever window context already exists.
        if (_contexts.empty())
            return make_failure(
                "OpenGL backend: render texture output requires an existing window context.");

        if (auto result = make_current(_contexts.front()); !result)
            return result;

        if (auto result = ensure_gl_loaded(); !result)
            return result;

        if (auto result = ensure_output_framebuffer(output_target.size); !result)
            return result;

        // Resource uploads (per-frame buffers, bind groups) require an active frame target, just
        // like the window path sets via ensure_frame_context. Texture frames borrow the context
        // host window, so record it as the current target or every upload this frame fails.
        _state.current_target = _contexts.front();
        clear_bound_state();
        _is_texture_frame = true;
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::end_frame()
    {
        clear_bound_state();
        auto result = consume_gl_errors("end_frame");
        _state.current_target = {};
        _is_texture_frame = false;
        return result;
    }

    tbx::Result OpenGlGraphicsBackend::present()
    {
        if (_is_texture_frame)
        {
            // Texture frames swap nothing, so the loop has no natural pacing. Mirror a vsynced
            // swap only when vsync is actually requested; with vsync off the loop runs unthrottled
            // exactly as a windowed swap would.
            if (_state.vsync_mode != tbx::VsyncMode::OFF)
            {
                const auto next_frame_time = _last_texture_present_time + TEXTURE_FRAME_INTERVAL;
                std::this_thread::sleep_until(next_frame_time);
            }
            _last_texture_present_time = std::chrono::steady_clock::now();
            return make_success();
        }

        if (!_state.current_target.id.is_valid())
            return make_failure("OpenGL backend: no active window to present.");

        if (!std::ranges::contains(_contexts, _state.current_target))
            return make_failure("OpenGL backend: active window context was not found.");

        return present(_state.current_target);
    }

    tbx::Result OpenGlGraphicsBackend::read_back_buffer(
        const tbx::Size& backbuffer_size,
        std::vector<uint8>& out_pixels)
    {
        if (!_state.is_loaded)
            return make_failure("OpenGL backend: cannot read the back buffer before GL is loaded.");

        const auto width = backbuffer_size.width;
        const auto height = backbuffer_size.height;
        if (width == 0U || height == 0U)
            return make_failure("OpenGL backend: cannot read a zero-sized back buffer.");

        const auto stride = static_cast<size>(width) * 4U;
        const auto buffer_bytes = stride * height;
        constexpr uint32 PBO_COUNT = 3U; // a ring deep enough to read a fence two frames behind

        if (_readback_pbos[0] == 0U)
            glGenBuffers(static_cast<GLsizei>(PBO_COUNT), _readback_pbos);

        // Resize every pixel buffer when the capture size changes; queued frames no longer match,
        // so drop their fences and start filling the ring again.
        if (_readback_pbo_size.width != width || _readback_pbo_size.height != height)
        {
            for (uint32 slot = 0U; slot < PBO_COUNT; ++slot)
            {
                glBindBuffer(GL_PIXEL_PACK_BUFFER, _readback_pbos[slot]);
                glBufferData(
                    GL_PIXEL_PACK_BUFFER,
                    static_cast<GLsizeiptr>(buffer_bytes),
                    nullptr,
                    GL_STREAM_READ);
                if (_readback_fences[slot] != nullptr)
                {
                    glDeleteSync(static_cast<GLsync>(_readback_fences[slot]));
                    _readback_fences[slot] = nullptr;
                }
            }
            _readback_pbo_size = backbuffer_size;
            _readback_write_index = 0U;
            _readback_inflight = 0U;
        }

        // The pipeline may leave an offscreen FBO bound; read explicitly from the chosen target
        // and restore whatever read/pack state was active afterwards.
        GLint previous_read_framebuffer = 0;
        GLint previous_pack_alignment = 4;
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previous_read_framebuffer);
        glGetIntegerv(GL_PACK_ALIGNMENT, &previous_pack_alignment);

        // Issue the read into the current ring buffer. With a pack buffer bound glReadPixels queues
        // an asynchronous DMA and returns immediately; a fence lets a later call tell when the copy
        // has finished without ever blocking the render lane.
        const auto write_index = _readback_write_index;
        if (_readback_fences[write_index] != nullptr) // a skipped read left this slot's fence behind
        {
            glDeleteSync(static_cast<GLsync>(_readback_fences[write_index]));
            _readback_fences[write_index] = nullptr;
        }
        glBindFramebuffer(GL_READ_FRAMEBUFFER, get_output_framebuffer());
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadBuffer(_is_texture_frame ? GL_COLOR_ATTACHMENT0 : GL_BACK);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, _readback_pbos[write_index]);
        glReadPixels(
            0,
            0,
            static_cast<GLsizei>(width),
            static_cast<GLsizei>(height),
            GL_BGRA,
            GL_UNSIGNED_BYTE,
            nullptr);
        _readback_fences[write_index] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        // Texture frames never swap, so nothing else submits this work to the GPU. Flush so the
        // readback and its fence actually start now and have finished by the time a later call polls.
        glFlush();

        // Deliver the buffer filled two calls ago, but only if its DMA has finished. Polling the
        // fence with a zero timeout never blocks: a not-yet-ready frame is simply skipped (the
        // viewport holds the previous frame a beat) instead of stalling the render lane.
        auto result = make_failure("OpenGL backend: readback frame not ready yet.");
        if (_readback_inflight >= 2U)
        {
            const auto read_index = (write_index + 1U) % PBO_COUNT;
            const auto fence = static_cast<GLsync>(_readback_fences[read_index]);
            if (fence != nullptr)
            {
                const auto status = glClientWaitSync(fence, 0, 0);
                if (status == GL_ALREADY_SIGNALED || status == GL_CONDITION_SATISFIED)
                {
                    glBindBuffer(GL_PIXEL_PACK_BUFFER, _readback_pbos[read_index]);
                    const auto* mapped = static_cast<const uint8*>(glMapBufferRange(
                        GL_PIXEL_PACK_BUFFER,
                        0,
                        static_cast<GLsizeiptr>(buffer_bytes),
                        GL_MAP_READ_BIT));
                    if (mapped != nullptr)
                    {
                        out_pixels.resize(buffer_bytes);
                        // GL rows are bottom-up; deliver top-down.
                        for (uint32 row = 0U; row < height; ++row)
                        {
                            const auto* src = mapped + static_cast<size>(height - 1U - row) * stride;
                            std::memcpy(
                                out_pixels.data() + static_cast<size>(row) * stride, src, stride);
                        }
                        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
                        result = make_success();
                    }
                    else
                    {
                        result = make_failure("OpenGL backend: failed to map the readback buffer.");
                    }
                    glDeleteSync(fence);
                    _readback_fences[read_index] = nullptr;
                }
            }
        }

        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0U);
        glPixelStorei(GL_PACK_ALIGNMENT, previous_pack_alignment);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previous_read_framebuffer));

        _readback_write_index = (write_index + 1U) % PBO_COUNT;
        if (_readback_inflight < PBO_COUNT)
            ++_readback_inflight;

        const auto gl_result = consume_gl_errors("read_back_buffer");
        if (!result)
            return result;
        return gl_result;
    }

    void OpenGlGraphicsBackend::wait_for_idle()
    {
        if (!_state.is_loaded)
            return;

        glFinish();
    }

    tbx::Result OpenGlGraphicsBackend::begin_render_pass(const tbx::RenderPassDesc& pass)
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

        if (!pass.color_targets.empty() || pass.depth_stencil_target != tbx::INVALID_GPU_ID)
        {
            _cache.pass_framebuffer = std::make_unique<OpenGlFramebuffer>();

            for (uint32 index = 0U; index < pass.color_targets.size(); ++index)
            {
                const auto texture_it = _cache.textures.find(pass.color_targets[index]);
                if (texture_it == _cache.textures.end())
                    return make_failure("OpenGL backend: render pass color target was not found.");

                _cache.pass_framebuffer->attach_color(index, texture_it->second.texture);
            }

            if (pass.depth_stencil_target != tbx::INVALID_GPU_ID)
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
            else if (pass.depth_stencil_target != tbx::INVALID_GPU_ID)
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
            glBindFramebuffer(GL_FRAMEBUFFER, get_output_framebuffer());
            glViewport(
                static_cast<GLint>(_state.current_viewport.position.x),
                static_cast<GLint>(_state.current_viewport.position.y),
                static_cast<GLsizei>(_state.current_viewport.dimensions.width),
                static_cast<GLsizei>(_state.current_viewport.dimensions.height));
        }

        GLbitfield clear_mask = 0U;
        if (has_clear_flag(pass.clear_flags, tbx::ClearFlags::COLOR))
        {
            glClearColor(
                pass.clear_color.r,
                pass.clear_color.g,
                pass.clear_color.b,
                pass.clear_color.a);
            clear_mask |= GL_COLOR_BUFFER_BIT;
        }
        if (has_clear_flag(pass.clear_flags, tbx::ClearFlags::DEPTH))
        {
            // Clear happens before pipeline state is applied for this pass; force depth writes on
            // so stale GL state from a previous pass cannot block the depth clear.
            glDepthMask(GL_TRUE);
            _state.has_current_pipeline_state = false;
            glClearDepth(pass.clear_depth);
            clear_mask |= GL_DEPTH_BUFFER_BIT;
        }
        if (has_clear_flag(pass.clear_flags, tbx::ClearFlags::STENCIL))
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
        glBindFramebuffer(GL_FRAMEBUFFER, get_output_framebuffer());
        _cache.pass_framebuffer.reset();
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::bind_raster_pipeline(
        const tbx::GpuId& pipeline_resource_uuid)
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

        _state.current_pipeline_state.id = pipeline_resource_uuid;
        _state.bound_vertex_buffers.clear();
        _state.bound_index_buffer = tbx::INVALID_GPU_ID;
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::bind_group(
        const uint32 set_index,
        const tbx::GpuId& group_resource_uuid)
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
            if (binding.resource == tbx::INVALID_GPU_ID)
                continue;

            switch (binding.type)
            {
                case OpenGlBindEntryType::VERTEX_BUFFER:
                {
                    if (_state.current_pipeline_state.id == tbx::INVALID_GPU_ID
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

                    if (const auto* cached =
                            find_bound_slot(_state.bound_vertex_buffers, binding.slot);
                        cached != nullptr && *cached == binding.resource)
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
                    set_bound_slot(_state.bound_vertex_buffers, binding.slot, binding.resource);
                    break;
                }
                case OpenGlBindEntryType::INDEX_BUFFER:
                {
                    if (_state.current_pipeline_state.id == tbx::INVALID_GPU_ID
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

                    if (const auto* cached =
                            find_bound_slot(_state.bound_uniform_buffers, binding.slot);
                        cached != nullptr
                        && is_same_buffer_slot_binding(
                            *cached,
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
                    set_bound_slot(
                        _state.bound_uniform_buffers,
                        binding.slot,
                        OpenGlBufferSlotBinding {
                            .resource = binding.resource,
                            .offset = binding.offset,
                            .range = binding.range});
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

                    if (const auto* cached =
                            find_bound_slot(_state.bound_storage_buffers, binding.slot);
                        cached != nullptr
                        && is_same_buffer_slot_binding(
                            *cached,
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
                    set_bound_slot(
                        _state.bound_storage_buffers,
                        binding.slot,
                        OpenGlBufferSlotBinding {
                            .resource = binding.resource,
                            .offset = binding.offset,
                            .range = binding.range});
                    break;
                }
                case OpenGlBindEntryType::SAMPLED_TEXTURE:
                {
                    const auto texture_it = _cache.textures.find(binding.resource);
                    if (texture_it == _cache.textures.end())
                        return make_failure("OpenGL backend: texture was not found.");

                    if (const auto* cached =
                            find_bound_slot(_state.bound_sampled_textures, binding.slot);
                        cached != nullptr && *cached == binding.resource)
                    {
                        break;
                    }

                    texture_it->second.texture.bind_slot(binding.slot);
                    set_bound_slot(_state.bound_sampled_textures, binding.slot, binding.resource);
                    break;
                }
                case OpenGlBindEntryType::SAMPLER:
                {
                    const auto sampler_it = _cache.samplers.find(binding.resource);
                    if (sampler_it == _cache.samplers.end())
                        return make_failure("OpenGL backend: sampler was not found.");

                    if (const auto* cached = find_bound_slot(_state.bound_samplers, binding.slot);
                        cached != nullptr && *cached == binding.resource)
                    {
                        break;
                    }

                    sampler_it->second.bind_slot(binding.slot);
                    set_bound_slot(_state.bound_samplers, binding.slot, binding.resource);
                    break;
                }
                default:
                    return make_failure(
                        "OpenGL backend: bind group entry type was not recognized.");
            }
        }

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
        if (_state.current_pipeline_state.id == tbx::INVALID_GPU_ID
            || pipeline_it == _cache.raster_pipelines.end())
        {
            return make_failure("OpenGL backend: no raster pipeline is currently bound.");
        }

        const auto& pipeline = pipeline_it->second;
        if (_state.bound_index_buffer != tbx::INVALID_GPU_ID)
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
        const tbx::GpuId& argument_buffer,
        const uint64 offset,
        const uint32 draw_count,
        const uint32 stride)
    {
        const auto pipeline_it = _cache.raster_pipelines.find(_state.current_pipeline_state.id);
        if (_state.current_pipeline_state.id == tbx::INVALID_GPU_ID
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
        if (_state.bound_index_buffer != tbx::INVALID_GPU_ID)
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
        return consume_gl_errors("draw_indirect");
    }

    tbx::Result OpenGlGraphicsBackend::destroy_resource(const tbx::GpuId& resource_uuid)
    {
        if (auto group_it = _cache.bind_groups.find(resource_uuid);
            group_it != _cache.bind_groups.end())
        {
            _cache.bind_groups.erase(group_it);
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

    tbx::Result OpenGlGraphicsBackend::create_bind_group(
        const tbx::BindGroupDesc& desc,
        tbx::GpuId& out_resource_uuid)
    {
        auto bind_entries = std::vector<OpenGlBindEntry>();
        bind_entries.reserve(desc.bindings.size());

        for (const auto& binding : desc.bindings)
        {
            if (binding.resource_handle == tbx::INVALID_GPU_ID)
                continue;

            // The renderer omits explicit layouts; resolve each binding's resource class from the
            // bound resource's creation usage.
            auto entry_type = OpenGlBindEntryType::UNIFORM_BUFFER;
            auto has_type = false;

            if (const auto buffer_it = _cache.buffers.find(binding.resource_handle);
                buffer_it != _cache.buffers.end())
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
                    return make_failure(
                        "OpenGL backend: bind group buffer usage was not recognized.");
                has_type = true;
            }

            if (!has_type && _cache.textures.contains(binding.resource_handle))
            {
                entry_type = OpenGlBindEntryType::SAMPLED_TEXTURE;
                has_type = true;
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

        out_resource_uuid = next_resource_id();
        _cache.bind_groups.emplace(out_resource_uuid, std::move(bind_entries));
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::create_buffer(
        const tbx::BufferDesc& desc,
        tbx::GpuId& out_resource_uuid)
    {
        if (auto result = require_gl_ready_for_resource_ops(); !result)
            return result;

        if (desc.size == 0U)
            return make_failure("OpenGL backend: buffer size must be greater than zero.");

        out_resource_uuid = next_resource_id();
        _cache.buffers.emplace(
            out_resource_uuid,
            OpenGlBufferResource {
                .buffer = OpenGlGraphicsBuffer(desc, nullptr, 0U),
                .size = desc.size,
                .is_vertex_buffer = has_buffer_usage(desc.usage, tbx::BufferUsage::VERTEX),
                .is_index_buffer = has_buffer_usage(desc.usage, tbx::BufferUsage::INDEX),
                .is_uniform_buffer =
                    has_buffer_usage(desc.usage, tbx::BufferUsage::UNIFORM),
                .is_storage_buffer =
                    has_buffer_usage(desc.usage, tbx::BufferUsage::STORAGE),
                .is_indirect_argument_buffer =
                    has_buffer_usage(desc.usage, tbx::BufferUsage::INDIRECT_ARGS),
            });
        if (auto result = consume_gl_errors("create_buffer"); !result)
        {
            _cache.buffers.erase(out_resource_uuid);
            out_resource_uuid = tbx::INVALID_GPU_ID;
            return result;
        }

        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::create_raster_pipeline(
        const tbx::RasterPipelineDesc& desc,
        tbx::GpuId& out_resource_uuid)
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

        out_resource_uuid = next_resource_id();
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
        const tbx::SamplerDesc& desc,
        tbx::GpuId& out_resource_uuid)
    {
        if (auto result = require_gl_ready_for_resource_ops(); !result)
            return result;

        out_resource_uuid = next_resource_id();
        _cache.samplers.emplace(out_resource_uuid, desc);
        if (auto result = consume_gl_errors("create_sampler"); !result)
        {
            _cache.samplers.erase(out_resource_uuid);
            out_resource_uuid = tbx::INVALID_GPU_ID;
            return result;
        }

        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::create_texture(
        const tbx::TextureDesc& desc,
        tbx::GpuId& out_resource_uuid)
    {
        if (auto result = require_gl_ready_for_resource_ops(); !result)
            return result;

        if (desc.size.width == 0U || desc.size.height == 0U)
            return make_failure("OpenGL backend: texture size must be greater than zero.");

        out_resource_uuid = next_resource_id();
        _cache.textures.emplace(
            out_resource_uuid,
            OpenGlTextureResource {
                .texture = OpenGlTexture(desc, nullptr),
                .size = desc.size,
                .bytes_per_pixel = get_texture_bytes_per_pixel(desc.format),
                .array_layer_count = std::max(desc.array_layer_count, 1U),
                .depth_attachment = get_depth_attachment(desc.format),
                .upload_format = get_texture_upload_format(desc.format),
                .upload_type = get_texture_upload_type(desc.format),
            });
        if (auto result = consume_gl_errors("create_texture"); !result)
        {
            _cache.textures.erase(out_resource_uuid);
            out_resource_uuid = tbx::INVALID_GPU_ID;
            return result;
        }

        return make_success();
    }

    bool OpenGlGraphicsBackend::supports_bindless_textures() const
    {
        // Bindless is assumed available on the semi-modern GPU/PC targets this backend supports.
        return true;
    }

    tbx::Result OpenGlGraphicsBackend::get_texture_bindless_handle(
        const tbx::GpuId& texture_uuid,
        uint64& out_handle)
    {
        out_handle = 0U;
        const auto iterator = _cache.textures.find(texture_uuid);
        if (iterator == _cache.textures.end())
            return make_failure("OpenGL backend: texture not found for bindless handle.");

        const GLuint64 handle = iterator->second.texture.get_or_create_bindless_handle();
        if (handle == 0U)
            return make_failure("OpenGL backend: failed to create bindless texture handle.");

        out_handle = static_cast<uint64>(handle);
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::write_buffer(
        const tbx::GpuId& resource_uuid,
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
        const tbx::GpuId& resource_uuid,
        const tbx::TextureUpdateDesc& desc,
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
        // Refresh the mip chain from the freshly uploaded base level. Only the base level carries
        // source pixels; smaller levels are derived here.
        if (desc.mip_level == 0U)
            texture.texture.generate_mipmaps();
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

    tbx::GpuId OpenGlGraphicsBackend::next_resource_id()
    {
        while (_next_resource_id == tbx::INVALID_GPU_ID
               || _cache.bind_groups.contains(_next_resource_id)
               || _cache.buffers.contains(_next_resource_id)
               || _cache.raster_pipelines.contains(_next_resource_id)
               || _cache.samplers.contains(_next_resource_id)
               || _cache.textures.contains(_next_resource_id))
        {
            ++_next_resource_id;
        }

        return _next_resource_id++;
    }

    void OpenGlGraphicsBackend::clear_bound_state()
    {
        _state.current_pipeline_state = {};
        _state.bound_index_buffer = tbx::INVALID_GPU_ID;
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
        glDisable(GL_POLYGON_OFFSET_FILL);
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
        _cache.bind_groups.clear();
        _cache.buffers.clear();
        _cache.samplers.clear();
        _cache.textures.clear();

        if (_readback_pbos[0] != 0U)
        {
            for (auto& fence : _readback_fences)
            {
                if (fence != nullptr)
                {
                    glDeleteSync(static_cast<GLsync>(fence));
                    fence = nullptr;
                }
            }
            glDeleteBuffers(3, _readback_pbos);
            _readback_pbos[0] = 0U;
            _readback_pbos[1] = 0U;
            _readback_pbos[2] = 0U;
            _readback_pbo_size = {};
            _readback_write_index = 0U;
            _readback_inflight = 0U;
        }

        clear_bound_state();
    }

    void OpenGlGraphicsBackend::apply_raster_pipeline_state(const OpenGlPipelineState& state)
    {
        if (_state.has_current_pipeline_state
            && _state.current_pipeline_state.depth_function == state.depth_function
            && _state.current_pipeline_state.is_depth_test_enabled == state.is_depth_test_enabled
            && _state.current_pipeline_state.is_depth_write_enabled == state.is_depth_write_enabled
            && _state.current_pipeline_state.is_blending_enabled == state.is_blending_enabled
            && _state.current_pipeline_state.is_culling_enabled == state.is_culling_enabled
            && _state.current_pipeline_state.depth_bias_constant == state.depth_bias_constant
            && _state.current_pipeline_state.depth_bias_slope == state.depth_bias_slope
            && _state.current_pipeline_state.cull_mode == state.cull_mode
            && _state.current_pipeline_state.blend_equation == state.blend_equation)
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
            || _state.current_pipeline_state.depth_function != state.depth_function)
        {
            glDepthFunc(to_gl_depth_function(state.depth_function));
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
            glCullFace(state.cull_mode == tbx::CullMode::FRONT ? GL_FRONT : GL_BACK);
        }
        if (!_state.has_current_pipeline_state
            || _state.current_pipeline_state.depth_bias_constant != state.depth_bias_constant
            || _state.current_pipeline_state.depth_bias_slope != state.depth_bias_slope)
        {
            const bool is_depth_bias_enabled =
                state.depth_bias_constant != 0.0F || state.depth_bias_slope != 0.0F;
            is_depth_bias_enabled ? glEnable(GL_POLYGON_OFFSET_FILL)
                                  : glDisable(GL_POLYGON_OFFSET_FILL);
            if (is_depth_bias_enabled)
                glPolygonOffset(state.depth_bias_slope, state.depth_bias_constant);
        }

        // Re-issue the blend func whenever blending turns on or the equation changes.
        //   ALPHA (final = src*src.a + dst*src.rgb): a colored composite — the surface adds its own
        //     alpha-weighted color AND tints (multiplies) whatever is behind it by its color, so an
        //     alpha-blended pane filters the background instead of just fading over it.
        //   MULTIPLY (final = dst*src): a pure colored filter; also used to accumulate the translucent
        //     shadow map's transmittance across stacked transparent casters.
        if (state.is_blending_enabled
            && (!_state.has_current_pipeline_state
                || !_state.current_pipeline_state.is_blending_enabled
                || _state.current_pipeline_state.blend_equation != state.blend_equation))
        {
            if (state.blend_equation == tbx::BlendEquation::MULTIPLY)
                glBlendFunc(GL_ZERO, GL_SRC_COLOR);
            else
                glBlendFunc(GL_SRC_ALPHA, GL_SRC_COLOR);
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

    tbx::Result OpenGlGraphicsBackend::ensure_output_framebuffer(const tbx::Size& output_size)
    {
        if (_output_framebuffer != 0U
            && _output_size.width == output_size.width
            && _output_size.height == output_size.height)
            return make_success();

        destroy_output_framebuffer();

        glGenTextures(1, &_output_color_texture);
        glBindTexture(GL_TEXTURE_2D, _output_color_texture);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA8,
            static_cast<GLsizei>(output_size.width),
            static_cast<GLsizei>(output_size.height),
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0U);

        glGenRenderbuffers(1, &_output_depth_renderbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, _output_depth_renderbuffer);
        glRenderbufferStorage(
            GL_RENDERBUFFER,
            GL_DEPTH24_STENCIL8,
            static_cast<GLsizei>(output_size.width),
            static_cast<GLsizei>(output_size.height));
        glBindRenderbuffer(GL_RENDERBUFFER, 0U);

        glGenFramebuffers(1, &_output_framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, _output_framebuffer);
        glFramebufferTexture2D(
            GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D,
            _output_color_texture,
            0);
        glFramebufferRenderbuffer(
            GL_FRAMEBUFFER,
            GL_DEPTH_STENCIL_ATTACHMENT,
            GL_RENDERBUFFER,
            _output_depth_renderbuffer);

        const auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glBindFramebuffer(GL_FRAMEBUFFER, 0U);
        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            destroy_output_framebuffer();
            return make_failure("OpenGL backend: render texture output framebuffer is incomplete.");
        }

        _output_size = output_size;
        return consume_gl_errors("ensure_output_framebuffer");
    }

    void OpenGlGraphicsBackend::destroy_output_framebuffer()
    {
        if (_output_framebuffer != 0U)
        {
            glDeleteFramebuffers(1, &_output_framebuffer);
            _output_framebuffer = 0U;
        }

        if (_output_color_texture != 0U)
        {
            glDeleteTextures(1, &_output_color_texture);
            _output_color_texture = 0U;
        }

        if (_output_depth_renderbuffer != 0U)
        {
            glDeleteRenderbuffers(1, &_output_depth_renderbuffer);
            _output_depth_renderbuffer = 0U;
        }

        _output_size = {};
    }

    uint32 OpenGlGraphicsBackend::get_output_framebuffer() const
    {
        return _is_texture_frame ? _output_framebuffer : 0U;
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
