#include "opengl_backend.h"
#include <chrono>
#include <cstdint>
#include <thread>
#include "tbx/systems/debugging/macros.h"
#include "tbx/types/typedefs.h"
#include "tbx/types/viewport.h"

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
// wingdi.h defines an ERROR macro that would rewrite the engine's TBX_TRACE_ERROR / LogLevel::ERROR
// tokens; drop it now that the Windows headers are in.
#undef ERROR

// WGL_NV_DX_interop2 entry points. The extension is part of the ICD, so it has no import library;
// the pointers are resolved through wglGetProcAddress the first time a shared target is created,
// while a GL context is current. Signatures and access tokens match the extension spec.
#ifndef WGL_ACCESS_WRITE_DISCARD_NV
#define WGL_ACCESS_READ_ONLY_NV 0x00000000
#define WGL_ACCESS_READ_WRITE_NV 0x00000001
#define WGL_ACCESS_WRITE_DISCARD_NV 0x00000002
#endif

using PFN_wglDXOpenDeviceNV = HANDLE(WINAPI*)(void* dxDevice);
using PFN_wglDXCloseDeviceNV = BOOL(WINAPI*)(HANDLE hDevice);
using PFN_wglDXRegisterObjectNV =
    HANDLE(WINAPI*)(HANDLE hDevice, void* dxObject, GLuint name, GLenum type, GLenum access);
using PFN_wglDXUnregisterObjectNV = BOOL(WINAPI*)(HANDLE hDevice, HANDLE hObject);
using PFN_wglDXLockObjectsNV = BOOL(WINAPI*)(HANDLE hDevice, GLint count, HANDLE* hObjects);
using PFN_wglDXUnlockObjectsNV = BOOL(WINAPI*)(HANDLE hDevice, GLint count, HANDLE* hObjects);

static PFN_wglDXOpenDeviceNV g_wglDXOpenDeviceNV = nullptr;
static PFN_wglDXCloseDeviceNV g_wglDXCloseDeviceNV = nullptr;
static PFN_wglDXRegisterObjectNV g_wglDXRegisterObjectNV = nullptr;
static PFN_wglDXUnregisterObjectNV g_wglDXUnregisterObjectNV = nullptr;
static PFN_wglDXLockObjectsNV g_wglDXLockObjectsNV = nullptr;
static PFN_wglDXUnlockObjectsNV g_wglDXUnlockObjectsNV = nullptr;
#endif

namespace opengl_rendering
{
    // Mirrors a 60Hz vsynced swap for render-texture frames.
    constexpr std::chrono::microseconds TEXTURE_FRAME_INTERVAL(16667);

#ifdef _WIN32
    // How long begin_frame waits to take the shared texture from the editor before giving up for
    // this frame. Short so a not-yet-consuming editor never stalls the render lane: the frame just
    // renders into a throwaway framebuffer and the next one retries.
    constexpr uint32 SHARED_TARGET_ACQUIRE_TIMEOUT_MS = 8U;
#endif

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
        destroy_all_shared_textures();
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

        _active_shared_texture = nullptr;
#ifdef _WIN32
        // A streamed view renders straight into its shared GPU texture (no readback) when the frame
        // output names one (its resource id). Take the texture from the editor first: producer
        // acquires keyed-mutex key 0, then locks the registered object for GL. A short acquire timeout
        // means a not-yet-consuming editor can never stall the lane — we just fall through to the
        // throwaway framebuffer below.
        if (const auto it = _shared_textures.find(output_target.id.value);
            it != _shared_textures.end())
        {
            auto& shared = it->second;
            auto* mutex = static_cast<IDXGIKeyedMutex*>(shared.keyed_mutex);
            if (mutex->AcquireSync(0U, SHARED_TARGET_ACQUIRE_TIMEOUT_MS) == S_OK)
            {
                // Only treat the texture as ours to draw into if GL actually locked it. On a lock
                // failure, release the keyed mutex back to the producer side (key 0, unchanged state)
                // so end_frame doesn't unlock/present a never-locked object, and fall through to the
                // throwaway framebuffer for this frame.
                if (g_wglDXLockObjectsNV(_interop_device, 1, &shared.gl_interop_object))
                {
                    shared.is_locked = true;
                    _active_shared_texture = &shared;
                    _state.current_target = _contexts.front();
                    clear_bound_state();
                    _is_texture_frame = true;
                    return make_success();
                }
                mutex->ReleaseSync(0U);
            }
        }
#endif

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
#ifdef _WIN32
        if (_active_shared_texture != nullptr)
        {
            // Flush GL work into the shared texture, release it back to D3D, then hand the texture
            // to the editor: producer releases keyed-mutex key 1 (the editor acquires 1 / releases
            // 0). wglDXUnlockObjectsNV inserts the GL/D3D sync, so the editor sees a complete frame.
            auto& shared = *_active_shared_texture;
            glFlush();
            if (shared.is_locked)
            {
                g_wglDXUnlockObjectsNV(_interop_device, 1, &shared.gl_interop_object);
                shared.is_locked = false;
            }
            static_cast<IDXGIKeyedMutex*>(shared.keyed_mutex)->ReleaseSync(1U);
            _active_shared_texture = nullptr;
        }
#endif
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

    tbx::Result OpenGlGraphicsBackend::read_buffer(
        const tbx::GpuId& resource_uuid,
        const tbx::BufferRegion& region,
        void* out_data)
    {
        if (auto result = require_gl_ready_for_resource_ops(); !result)
            return result;

        if (region.size == 0U)
            return make_success();
        if (!out_data)
            return make_failure("OpenGL backend: buffer read destination is null.");

        const auto buffer_it = _cache.buffers.find(resource_uuid);
        if (buffer_it == _cache.buffers.end())
            return make_failure("OpenGL backend: buffer was not found.");
        if (region.offset + region.size > buffer_it->second.size)
            return make_failure("OpenGL backend: buffer read exceeds buffer size.");

        glGetNamedBufferSubData(
            buffer_it->second.buffer.get_buffer_id(),
            static_cast<GLintptr>(region.offset),
            static_cast<GLsizeiptr>(region.size),
            out_data);
        return consume_gl_errors("read_buffer");
    }

    tbx::Result OpenGlGraphicsBackend::read_texture(
        const tbx::GpuId& resource_uuid,
        const tbx::TextureRegion& region,
        void* out_data)
    {
        if (!_state.is_loaded)
            return make_failure("OpenGL backend: cannot read a texture before GL is loaded.");
        if (!out_data)
            return make_failure("OpenGL backend: texture read destination is null.");

        // A null resource reads the active frame output as BGRA8 top-down rows: a one-shot synchronous
        // capture for headless --screenshot. There is no addressable texture resource behind the
        // window/offscreen backbuffer, and the editor never takes this path (it samples shared
        // textures directly), so a plain blocking read is fine — no PBO ring.
        if (resource_uuid == tbx::INVALID_GPU_ID)
        {
            const auto width = region.width;
            const auto height = region.height;
            if (width == 0U || height == 0U)
                return make_failure("OpenGL backend: cannot read a zero-sized frame output.");

            const auto stride = static_cast<size>(width) * 4U;
            const auto buffer_bytes = stride * height;

            GLint previous_read_framebuffer = 0;
            GLint previous_pack_alignment = 4;
            glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previous_read_framebuffer);
            glGetIntegerv(GL_PACK_ALIGNMENT, &previous_pack_alignment);

            auto scratch = std::vector<uint8>(buffer_bytes);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, get_output_framebuffer());
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadBuffer(_is_texture_frame ? GL_COLOR_ATTACHMENT0 : GL_BACK);
            glReadPixels(
                0,
                0,
                static_cast<GLsizei>(width),
                static_cast<GLsizei>(height),
                GL_BGRA,
                GL_UNSIGNED_BYTE,
                scratch.data());

            glPixelStorei(GL_PACK_ALIGNMENT, previous_pack_alignment);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previous_read_framebuffer));

            auto* out_bytes = static_cast<uint8*>(out_data);
            // GL rows are bottom-up; deliver top-down.
            for (uint32 row = 0U; row < height; ++row)
            {
                const auto* src = scratch.data() + static_cast<size>(height - 1U - row) * stride;
                std::memcpy(out_bytes + static_cast<size>(row) * stride, src, stride);
            }
            return consume_gl_errors("read_texture");
        }

        const auto texture_it = _cache.textures.find(resource_uuid);
        if (texture_it == _cache.textures.end())
            return make_failure("OpenGL backend: texture was not found.");

        const auto& texture = texture_it->second;
        if (region.x + region.width > texture.size.width
            || region.y + region.height > texture.size.height)
            return make_failure("OpenGL backend: texture read exceeds texture bounds.");
        if (region.array_layer >= texture.array_layer_count)
            return make_failure("OpenGL backend: texture read array layer is out of bounds.");

        // The region plus the texture's format determine the byte count; out_data holds exactly that.
        const uint64 region_byte_size = static_cast<uint64>(region.width)
                                        * static_cast<uint64>(region.height)
                                        * texture.bytes_per_pixel;

        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glGetTextureSubImage(
            texture.texture.get_texture_id(),
            static_cast<GLint>(region.mip_level),
            static_cast<GLint>(region.x),
            static_cast<GLint>(region.y),
            static_cast<GLint>(region.array_layer),
            static_cast<GLsizei>(region.width),
            static_cast<GLsizei>(region.height),
            1,
            texture.upload_format,
            texture.upload_type,
            static_cast<GLsizei>(region_byte_size),
            out_data);
        return consume_gl_errors("read_texture");
    }

#ifdef _WIN32
    tbx::Result OpenGlGraphicsBackend::ensure_d3d_interop_ready()
    {
        if (_interop_device != nullptr)
            return make_success();

        // The interop functions live in the ICD, so they are resolved through wglGetProcAddress
        // (which needs a current GL context — the caller establishes one first).
        if (g_wglDXOpenDeviceNV == nullptr)
        {
            g_wglDXOpenDeviceNV =
                reinterpret_cast<PFN_wglDXOpenDeviceNV>(wglGetProcAddress("wglDXOpenDeviceNV"));
            g_wglDXCloseDeviceNV =
                reinterpret_cast<PFN_wglDXCloseDeviceNV>(wglGetProcAddress("wglDXCloseDeviceNV"));
            g_wglDXRegisterObjectNV = reinterpret_cast<PFN_wglDXRegisterObjectNV>(
                wglGetProcAddress("wglDXRegisterObjectNV"));
            g_wglDXUnregisterObjectNV = reinterpret_cast<PFN_wglDXUnregisterObjectNV>(
                wglGetProcAddress("wglDXUnregisterObjectNV"));
            g_wglDXLockObjectsNV =
                reinterpret_cast<PFN_wglDXLockObjectsNV>(wglGetProcAddress("wglDXLockObjectsNV"));
            g_wglDXUnlockObjectsNV = reinterpret_cast<PFN_wglDXUnlockObjectsNV>(
                wglGetProcAddress("wglDXUnlockObjectsNV"));
        }
        if (g_wglDXOpenDeviceNV == nullptr || g_wglDXCloseDeviceNV == nullptr
            || g_wglDXRegisterObjectNV == nullptr || g_wglDXUnregisterObjectNV == nullptr
            || g_wglDXLockObjectsNV == nullptr || g_wglDXUnlockObjectsNV == nullptr)
            return make_failure(
                "OpenGL backend: WGL_NV_DX_interop2 is unavailable; GPU texture sharing needs a "
                "driver that exports it on the same GPU adapter as the editor.");

        if (_d3d_device == nullptr)
        {
            ID3D11Device* device = nullptr;
            ID3D11DeviceContext* context = nullptr;
            const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
            D3D_FEATURE_LEVEL obtained = {};
            const auto hr = ::D3D11CreateDevice(
                nullptr, // default adapter — must be the same GPU the editor composites on
                D3D_DRIVER_TYPE_HARDWARE,
                nullptr,
                D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                levels,
                ARRAYSIZE(levels),
                D3D11_SDK_VERSION,
                &device,
                &obtained,
                &context);
            if (FAILED(hr) || device == nullptr)
                return make_failure(
                    "OpenGL backend: failed to create the D3D11 device for texture sharing.");
            _d3d_device = device;
            _d3d_context = context;
        }

        const auto interop = g_wglDXOpenDeviceNV(_d3d_device);
        if (interop == nullptr)
            return make_failure(
                "OpenGL backend: wglDXOpenDeviceNV failed to open the D3D11 device for interop.");
        _interop_device = interop;
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::create_shared_texture(
        const tbx::TextureDesc& texture_desc,
        const tbx::GpuId id)
    {
        const auto size = texture_desc.size;
        if (size.width == 0U || size.height == 0U)
            return make_failure("OpenGL backend: shared texture size is zero.");
        if (_contexts.empty())
            return make_failure(
                "OpenGL backend: a window context must exist before creating a shared texture.");

        if (auto result = make_current(_contexts.front()); !result)
            return result;
        if (auto result = ensure_gl_loaded(); !result)
            return result;
        if (auto result = ensure_d3d_interop_ready(); !result)
            return result;

        auto* device = static_cast<ID3D11Device*>(_d3d_device);

        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = size.width;
        desc.Height = size.height;
        desc.MipLevels = 1U;
        desc.ArraySize = 1U;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1U;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        // Keyed mutex + a global cross-process handle is exactly what the editor's compositor
        // imports (Avalonia's D3D11TextureGlobalSharedHandle).
        desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

        ID3D11Texture2D* texture = nullptr;
        if (FAILED(device->CreateTexture2D(&desc, nullptr, &texture)) || texture == nullptr)
            return make_failure("OpenGL backend: failed to create the shared D3D11 texture.");

        HANDLE share_handle = nullptr;
        IDXGIResource* dxgi_resource = nullptr;
        if (FAILED(texture->QueryInterface(
                __uuidof(IDXGIResource), reinterpret_cast<void**>(&dxgi_resource)))
            || dxgi_resource == nullptr)
        {
            texture->Release();
            return make_failure("OpenGL backend: shared texture exposes no IDXGIResource.");
        }
        const auto handle_hr = dxgi_resource->GetSharedHandle(&share_handle);
        dxgi_resource->Release();
        if (FAILED(handle_hr) || share_handle == nullptr)
        {
            texture->Release();
            return make_failure("OpenGL backend: failed to obtain the shared texture handle.");
        }

        IDXGIKeyedMutex* keyed_mutex = nullptr;
        if (FAILED(texture->QueryInterface(
                __uuidof(IDXGIKeyedMutex), reinterpret_cast<void**>(&keyed_mutex)))
            || keyed_mutex == nullptr)
        {
            texture->Release();
            return make_failure("OpenGL backend: shared texture exposes no keyed mutex.");
        }

        // Register the D3D texture with GL as a renderbuffer (not a texture: a renderbuffer
        // attachment sidesteps an NVIDIA GL_FRAMEBUFFER_UNSUPPORTED bug) and hang it plus a private
        // depth-stencil buffer off a dedicated FBO this backend draws the view into.
        GLuint color_rb = 0U;
        GLuint depth_rb = 0U;
        GLuint fbo = 0U;
        glGenRenderbuffers(1, &color_rb);

        // Non-const so &gl_object is a mutable HANDLE* for the lock/unlock calls below.
        auto gl_object = g_wglDXRegisterObjectNV(
            _interop_device, texture, color_rb, GL_RENDERBUFFER, WGL_ACCESS_WRITE_DISCARD_NV);
        if (gl_object == nullptr)
        {
            glDeleteRenderbuffers(1, &color_rb);
            keyed_mutex->Release();
            texture->Release();
            return make_failure(
                "OpenGL backend: wglDXRegisterObjectNV failed for the shared texture.");
        }

        glGenRenderbuffers(1, &depth_rb);
        glBindRenderbuffer(GL_RENDERBUFFER, depth_rb);
        glRenderbufferStorage(
            GL_RENDERBUFFER,
            GL_DEPTH24_STENCIL8,
            static_cast<GLsizei>(size.width),
            static_cast<GLsizei>(size.height));
        glBindRenderbuffer(GL_RENDERBUFFER, 0U);

        // The color renderbuffer only has the texture's storage while the interop object is locked,
        // so lock for the completeness check, then unlock — per-frame locking lives in begin_frame.
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        g_wglDXLockObjectsNV(_interop_device, 1, &gl_object);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color_rb);
        glFramebufferRenderbuffer(
            GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depth_rb);
        const auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        g_wglDXUnlockObjectsNV(_interop_device, 1, &gl_object);
        glBindFramebuffer(GL_FRAMEBUFFER, 0U);

        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            g_wglDXUnregisterObjectNV(_interop_device, gl_object);
            glDeleteRenderbuffers(1, &color_rb);
            glDeleteRenderbuffers(1, &depth_rb);
            glDeleteFramebuffers(1, &fbo);
            keyed_mutex->Release();
            texture->Release();
            return make_failure("OpenGL backend: shared render target framebuffer is incomplete.");
        }

        auto shared = SharedTexture();
        shared.d3d_texture = texture;
        shared.keyed_mutex = keyed_mutex;
        shared.gl_interop_object = gl_object;
        shared.share_handle = share_handle;
        shared.color_renderbuffer = color_rb;
        shared.depth_renderbuffer = depth_rb;
        shared.framebuffer = fbo;
        shared.size = size;
        _shared_textures[id] = shared;

        return consume_gl_errors("create_shared_texture");
    }

    void OpenGlGraphicsBackend::release_shared_texture(SharedTexture& target)
    {
        if (target.gl_interop_object != nullptr && _interop_device != nullptr)
        {
            if (target.is_locked && g_wglDXUnlockObjectsNV != nullptr)
                g_wglDXUnlockObjectsNV(_interop_device, 1, &target.gl_interop_object);
            if (g_wglDXUnregisterObjectNV != nullptr)
                g_wglDXUnregisterObjectNV(_interop_device, target.gl_interop_object);
        }
        target.is_locked = false;

        if (target.framebuffer != 0U)
            glDeleteFramebuffers(1, &target.framebuffer);
        if (target.color_renderbuffer != 0U)
            glDeleteRenderbuffers(1, &target.color_renderbuffer);
        if (target.depth_renderbuffer != 0U)
            glDeleteRenderbuffers(1, &target.depth_renderbuffer);
        target.framebuffer = 0U;
        target.color_renderbuffer = 0U;
        target.depth_renderbuffer = 0U;

        if (target.keyed_mutex != nullptr)
            static_cast<IDXGIKeyedMutex*>(target.keyed_mutex)->Release();
        if (target.d3d_texture != nullptr)
            static_cast<ID3D11Texture2D*>(target.d3d_texture)->Release();
        target.keyed_mutex = nullptr;
        target.d3d_texture = nullptr;
        target.gl_interop_object = nullptr;
        target.share_handle = nullptr;
    }

    void OpenGlGraphicsBackend::destroy_all_shared_textures()
    {
        // release_shared_texture issues GL deletes (framebuffers/renderbuffers), so it needs a current
        // context. cleanup() can call this after the lane's context is no longer current, so make one
        // current explicitly.
        if (!_contexts.empty())
            (void)make_current(_contexts.front());

        for (auto& [id, texture] : _shared_textures)
            release_shared_texture(texture);
        _shared_textures.clear();
        _active_shared_texture = nullptr;

        if (_interop_device != nullptr && g_wglDXCloseDeviceNV != nullptr)
            g_wglDXCloseDeviceNV(_interop_device);
        _interop_device = nullptr;
        if (_d3d_context != nullptr)
            static_cast<ID3D11DeviceContext*>(_d3d_context)->Release();
        if (_d3d_device != nullptr)
            static_cast<ID3D11Device*>(_d3d_device)->Release();
        _d3d_context = nullptr;
        _d3d_device = nullptr;
    }
#else
    tbx::Result OpenGlGraphicsBackend::ensure_d3d_interop_ready()
    {
        return make_failure("OpenGL backend: GPU texture sharing is only implemented on Windows.");
    }

    tbx::Result OpenGlGraphicsBackend::create_shared_texture(const tbx::TextureDesc&, tbx::GpuId)
    {
        return make_failure("OpenGL backend: GPU texture sharing is only implemented on Windows.");
    }

    void OpenGlGraphicsBackend::release_shared_texture(SharedTexture&)
    {
    }

    void OpenGlGraphicsBackend::destroy_all_shared_textures()
    {
    }
#endif

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
        if (auto shared_it = _shared_textures.find(resource_uuid);
            shared_it != _shared_textures.end())
        {
            // release_shared_texture issues GL deletes, so make a context current explicitly.
            if (!_contexts.empty())
                (void)make_current(_contexts.front());
            if (_active_shared_texture == &shared_it->second)
                _active_shared_texture = nullptr;
            release_shared_texture(shared_it->second);
            _shared_textures.erase(shared_it);
            return make_success();
        }

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

        // A shared texture is a cross-process surface (D3D11 + keyed mutex) the backend renders into
        // when a frame's output names it; it lives in its own map, not the sampleable texture cache.
        if (desc.is_shared)
        {
            out_resource_uuid = next_resource_id();
            if (auto result = create_shared_texture(desc, out_resource_uuid); !result)
            {
                out_resource_uuid = tbx::INVALID_GPU_ID;
                return result;
            }
            return make_success();
        }

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

    tbx::Result OpenGlGraphicsBackend::get_gpu_handle(
        const tbx::GpuId& texture_uuid,
        uint64& out_handle)
    {
        out_handle = 0U;

        // A shared texture exposes its OS-global cross-process share handle; a normal sampled texture
        // exposes a resident ARB_bindless_texture handle indexable from shaders by uint.
        if (const auto shared_it = _shared_textures.find(texture_uuid);
            shared_it != _shared_textures.end())
        {
            out_handle =
                static_cast<uint64>(reinterpret_cast<uintptr_t>(shared_it->second.share_handle));
            if (out_handle == 0U)
                return make_failure("OpenGL backend: shared texture has no cross-process handle.");
            return make_success();
        }

        const auto iterator = _cache.textures.find(texture_uuid);
        if (iterator == _cache.textures.end())
            return make_failure("OpenGL backend: texture not found for GPU handle.");

        const GLuint64 handle = iterator->second.texture.get_or_create_bindless_handle();
        if (handle == 0U)
            return make_failure("OpenGL backend: failed to create bindless texture handle.");

        out_handle = static_cast<uint64>(handle);
        return make_success();
    }

    tbx::Result OpenGlGraphicsBackend::write_buffer(
        const tbx::GpuId& resource_uuid,
        const tbx::BufferRegion& region,
        const void* data)
    {
        if (auto result = require_gl_ready_for_resource_ops(); !result)
            return result;

        const auto buffer_it = _cache.buffers.find(resource_uuid);
        if (buffer_it == _cache.buffers.end())
            return make_failure("OpenGL backend: buffer was not found.");

        if (region.size > 0U && !data)
            return make_failure("OpenGL backend: buffer update data is null.");

        if (region.offset + region.size > buffer_it->second.size)
            return make_failure("OpenGL backend: buffer update exceeds buffer size.");

        buffer_it->second.buffer.update(data, region.size, region.offset);
        return consume_gl_errors("update_buffer");
    }

    tbx::Result OpenGlGraphicsBackend::write_texture(
        const tbx::GpuId& resource_uuid,
        const tbx::TextureRegion& region,
        const void* data)
    {
        if (auto result = require_gl_ready_for_resource_ops(); !result)
            return result;

        const auto texture_it = _cache.textures.find(resource_uuid);
        if (texture_it == _cache.textures.end())
            return make_failure("OpenGL backend: texture was not found.");

        if (region.width > 0U && region.height > 0U && !data)
            return make_failure("OpenGL backend: texture update data is null.");

        const auto& texture = texture_it->second;
        if (region.x + region.width > texture.size.width
            || region.y + region.height > texture.size.height)
            return make_failure("OpenGL backend: texture update exceeds texture bounds.");
        if (region.array_layer >= texture.array_layer_count)
            return make_failure("OpenGL backend: texture update array layer is out of bounds.");

        texture.texture.update(region, texture.upload_format, texture.upload_type, data);
        // Refresh the mip chain from the freshly uploaded base level. Only the base level carries
        // source pixels; smaller levels are derived here.
        if (region.mip_level == 0U)
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
               || _cache.textures.contains(_next_resource_id)
               || _shared_textures.contains(_next_resource_id))
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
#ifdef _WIN32
        // A streamed view draws into its shared texture's framebuffer; everything else keeps the
        // window backbuffer (0) or the throwaway texture framebuffer.
        if (_active_shared_texture != nullptr)
            return _active_shared_texture->framebuffer;
#endif
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
