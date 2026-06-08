#include "sdl_opengl_context_manager.h"
#include "tbx/interfaces/opengl_context_backend.h"
#include "tbx/systems/debugging/macros.h"

namespace sdl_opengl_context_manager
{
    static void try_release_current_context(SDL_GLContext context)
    {
        if (!context)
            return;
        if ((SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) == 0U)
            return;

        SDL_GLContext current_context = SDL_GL_GetCurrentContext();
        if (current_context != context)
            return;

        if (!SDL_GL_MakeCurrent(nullptr, nullptr))
        {
            const char* error = SDL_GetError();
            if (error && std::string_view(error) == "OpenGL not initialized")
            {
                SDL_ClearError();
                return;
            }

            TBX_TRACE_WARNING(
                "Failed to release current SDL OpenGL context before destruction: {}",
                error);
            SDL_ClearError();
        }
    }

    static void set_opengl_attribute(SDL_GLAttr attribute, int value)
    {
        if (!SDL_GL_SetAttribute(attribute, value))
        {
            TBX_TRACE_WARNING(
                "Failed to set SDL OpenGL attribute {}: {}",
                static_cast<int>(attribute),
                SDL_GetError());
        }
    }

    static bool is_sdl_video_initialized()
    {
        return (SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) != 0U;
    }

    SdlOpenGlContextBackend::~SdlOpenGlContextBackend() noexcept
    {
        shutdown();
    }

    void SdlOpenGlContextBackend::initialize(
        const int major_version,
        const int minor_version,
        const int depth_bits,
        const int stencil_bits,
        const bool double_buffer_enabled,
        const bool debug_context_enabled,
        const bool vsync_enabled)
    {
        _settings.major_version = major_version;
        _settings.minor_version = minor_version;
        _settings.depth_bits = depth_bits;
        _settings.stencil_bits = stencil_bits;
        _settings.is_double_buffer_enabled = double_buffer_enabled;
        _settings.is_debug_context_enabled = debug_context_enabled;
        _settings.vsync_mode = vsync_enabled ? tbx::VsyncMode::ON : tbx::VsyncMode::OFF;

        if (is_sdl_video_initialized())
            apply_default_attributes();
        apply_vsync_setting();
    }

    tbx::Result SdlOpenGlContextBackend::create_context(const tbx::Window& window)
    {
        TBX_ASSERT(window.id.is_valid(), "SDL OpenGL context manager requires a valid window id.");
        if (!window.id.is_valid())
            return make_failure("SDL OpenGL context manager: window id is invalid.");

        auto* sdl_window = get_sdl_window(window);
        if (!sdl_window)
            return make_failure("SDL OpenGL context manager: native window not available.");

        const std::string label = std::format("{}", window);
        if (!try_create_context(window, sdl_window, label))
            return make_failure("SDL OpenGL context manager: failed to create context.");

        auto result = tbx::Result();
        result.flag_success();
        return result;
    }

    tbx::Result SdlOpenGlContextBackend::destroy_context(const tbx::Window& window)
    {
        TBX_ASSERT(window.id.is_valid(), "SDL OpenGL context manager requires a valid window id.");
        if (!window.id.is_valid())
            return make_failure("SDL OpenGL context manager: window id is invalid.");

        destroy_native_context(window);

        auto result = tbx::Result();
        result.flag_success();
        return result;
    }

    tbx::Result SdlOpenGlContextBackend::make_context_current(const tbx::Window& window)
    {
        TBX_ASSERT(window.id.is_valid(), "SDL OpenGL context manager requires a valid window id.");
        if (!window.id.is_valid())
            return make_failure("SDL OpenGL context manager: window id is invalid.");

        auto* sdl_window = get_sdl_window(window);
        if (!sdl_window)
            return make_failure("SDL OpenGL context manager: native window not available.");

        const std::string label = std::format("{}", window);
        const auto context_it = _contexts.find(window);
        if (context_it == _contexts.end())
            return make_failure("SDL OpenGL context manager: context not created.");

        if (!try_make_current(sdl_window, context_it->second, label))
            return make_failure(SDL_GetError());

        auto result = tbx::Result();
        result.flag_success();
        return result;
    }

    tbx::Result SdlOpenGlContextBackend::swap_buffers(const tbx::Window& window)
    {
        TBX_ASSERT(window.id.is_valid(), "SDL OpenGL context manager requires a valid window id.");
        if (!window.id.is_valid())
            return make_failure("SDL OpenGL context manager: window id is invalid.");

        auto* sdl_window = get_sdl_window(window);
        if (!sdl_window)
            return make_failure("SDL OpenGL context manager: native window not available.");

        if (!try_present(window, sdl_window))
            return make_failure("SDL OpenGL context manager: present failed.");

        auto result = tbx::Result();
        result.flag_success();
        return result;
    }

    tbx::Result SdlOpenGlContextBackend::set_vsync(const tbx::VsyncMode& mode)
    {
        _settings.vsync_mode = mode;
        apply_vsync_setting();

        auto result = tbx::Result();
        result.flag_success();
        return result;
    }

    void SdlOpenGlContextBackend::shutdown()
    {
        for (const auto& context_entry : _contexts)
        {
            if (!context_entry.second)
                continue;

            try_release_current_context(context_entry.second);
            SDL_GL_DestroyContext(context_entry.second);
        }

        _contexts.clear();
    }

    tbx::GraphicsProcAddress SdlOpenGlContextBackend::get_proc_address() const
    {
        return reinterpret_cast<tbx::GraphicsProcAddress>(SDL_GL_GetProcAddress);
    }

    void SdlOpenGlContextBackend::apply_default_attributes() const
    {
        set_opengl_attribute(SDL_GL_CONTEXT_MAJOR_VERSION, _settings.major_version);
        set_opengl_attribute(SDL_GL_CONTEXT_MINOR_VERSION, _settings.minor_version);
        set_opengl_attribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        set_opengl_attribute(SDL_GL_DEPTH_SIZE, _settings.depth_bits);
        set_opengl_attribute(SDL_GL_STENCIL_SIZE, _settings.stencil_bits);
        set_opengl_attribute(SDL_GL_DOUBLEBUFFER, _settings.is_double_buffer_enabled ? 1 : 0);
        set_opengl_attribute(
            SDL_GL_CONTEXT_FLAGS,
            _settings.is_debug_context_enabled ? SDL_GL_CONTEXT_DEBUG_FLAG : 0);
    }

    int SdlOpenGlContextBackend::get_swap_interval() const
    {
        switch (_settings.vsync_mode)
        {
            case tbx::VsyncMode::ON:
                return 1;
            case tbx::VsyncMode::ADAPTIVE:
                return -1;
            case tbx::VsyncMode::OFF:
            default:
                return 0;
        }
    }

    tbx::Result SdlOpenGlContextBackend::make_failure(std::string message) const
    {
        auto result = tbx::Result();
        result.flag_failure(std::move(message));
        return result;
    }

    SDL_Window* SdlOpenGlContextBackend::get_sdl_window(const tbx::Window& window) const
    {
        return static_cast<SDL_Window*>(window.native_handle);
    }

    bool SdlOpenGlContextBackend::try_create_context(
        const tbx::Window& window,
        SDL_Window* sdl_window,
        const std::string& window_title)
    {
        if (!sdl_window)
            return false;

        const auto existing_context = _contexts.find(window);
        if (existing_context != _contexts.end())
            return try_make_current(sdl_window, existing_context->second, window_title);

        apply_default_attributes();
        SDL_GLContext context = SDL_GL_CreateContext(sdl_window);
        if (!context)
        {
            TBX_TRACE_ERROR(
                "Failed to create SDL OpenGL context for window '{}': {}",
                window_title,
                SDL_GetError());
            return false;
        }

        _contexts[window] = context;
        if (!SDL_GL_MakeCurrent(sdl_window, context))
        {
            TBX_TRACE_ERROR(
                "Failed to make SDL OpenGL context current for window '{}': {}",
                window_title,
                SDL_GetError());
            destroy_native_context(window);
            return false;
        }

        SDL_ClearError();
        if (!SDL_GL_SetSwapInterval(get_swap_interval()))
        {
            TBX_TRACE_WARNING(
                "Failed to set vsync={} for window '{}': {}",
                static_cast<int>(_settings.vsync_mode),
                window_title,
                SDL_GetError());
            SDL_ClearError();
        }

        return true;
    }

    void SdlOpenGlContextBackend::destroy_native_context(const tbx::Window& window)
    {
        if (!window.id.is_valid())
            return;

        auto context_it = _contexts.find(window);
        if (context_it == _contexts.end())
            return;

        if (context_it->second)
        {
            try_release_current_context(context_it->second);
            SDL_GL_DestroyContext(context_it->second);
        }
        _contexts.erase(context_it);
    }

    bool SdlOpenGlContextBackend::try_make_current(
        SDL_Window* sdl_window,
        SDL_GLContext context,
        const std::string& window_title)
    {
        if (!sdl_window || !context)
            return false;

        if (!SDL_GL_MakeCurrent(sdl_window, context))
        {
            TBX_TRACE_ERROR(
                "Failed to make OpenGL context current for window '{}': {}",
                window_title,
                SDL_GetError());
            return false;
        }

        return true;
    }

    bool SdlOpenGlContextBackend::try_present(const tbx::Window& window, SDL_Window* sdl_window)
    {
        if (!sdl_window || !_contexts.contains(window))
            return false;

        SDL_GL_SwapWindow(sdl_window);
        return true;
    }

    void SdlOpenGlContextBackend::apply_vsync_setting()
    {
        SDL_Window* current_window = SDL_GL_GetCurrentWindow();
        SDL_GLContext current_context = SDL_GL_GetCurrentContext();

        for (const auto& [window, context] : _contexts)
        {
            SDL_Window* sdl_window = get_sdl_window(window);
            if (!sdl_window || !context)
                continue;

            if (!SDL_GL_MakeCurrent(sdl_window, context))
            {
                TBX_TRACE_WARNING(
                    "Failed to make OpenGL context current to apply vsync: {}",
                    SDL_GetError());
                continue;
            }

            SDL_ClearError();
            if (!SDL_GL_SetSwapInterval(get_swap_interval()))
            {
                TBX_TRACE_WARNING(
                    "Failed to set vsync={}: {}",
                    static_cast<int>(_settings.vsync_mode),
                    SDL_GetError());
                SDL_ClearError();
            }
        }

        if (current_window && current_context)
            SDL_GL_MakeCurrent(current_window, current_context);
    }
}
