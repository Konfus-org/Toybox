#include "tbx/plugins/sdl_opengl_context_manager/sdl_opengl_context_manager.h"
#include "tbx/debugging/macros.h"
#include <utility>

namespace sdl_opengl_context_manager
{
    static void try_release_current_context(SDL_GLContext context)
    {
        if (!context)
            return;

        SDL_GLContext current_context = SDL_GL_GetCurrentContext();
        if (current_context != context)
            return;

        if (!SDL_GL_MakeCurrent(nullptr, nullptr))
        {
            TBX_TRACE_WARNING(
                "Failed to release current SDL OpenGL context before destruction: {}",
                SDL_GetError());
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

    SdlOpenGlContextManager::SdlOpenGlContextManager() = default;

    SdlOpenGlContextManager::~SdlOpenGlContextManager() noexcept
    {
        for (const auto& context_entry : _contexts)
        {
            if (context_entry.second)
            {
                try_release_current_context(context_entry.second);
                SDL_GL_DestroyContext(context_entry.second);
            }
        }
        _contexts.clear();
    }

    void SdlOpenGlContextManager::initialize(
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

        apply_default_attributes();
        apply_vsync_setting();
    }

    tbx::Result SdlOpenGlContextManager::make_current(const tbx::Window& window)
    {
        TBX_ASSERT(window.is_valid(), "SDL OpenGL context manager requires a valid window id.");
        if (!window.is_valid())
            return make_failure("SDL OpenGL context manager: window id is invalid.");

        const auto window_it = _native_windows.find(window);
        if (window_it == _native_windows.end() || !window_it->second)
            return make_failure("SDL OpenGL context manager: native window not available.");

        const std::string label = to_string(window);
        if (!_contexts.contains(window_it->second)
            && !try_create_context(window_it->second, label))
        {
            return make_failure("SDL OpenGL context manager: failed to create context.");
        }

        if (!try_make_current(window_it->second, label))
            return make_failure(SDL_GetError());

        auto result = tbx::Result {};
        result.flag_success();
        return result;
    }

    tbx::Result SdlOpenGlContextManager::present(const tbx::Window& window)
    {
        TBX_ASSERT(window.is_valid(), "SDL OpenGL context manager requires a valid window id.");
        if (!window.is_valid())
            return make_failure("SDL OpenGL context manager: window id is invalid.");

        const auto window_it = _native_windows.find(window);
        if (window_it == _native_windows.end() || !window_it->second)
            return make_failure("SDL OpenGL context manager: native window not available.");

        if (!try_present(window_it->second))
            return make_failure("SDL OpenGL context manager: present failed.");

        auto result = tbx::Result {};
        result.flag_success();
        return result;
    }

    tbx::Result SdlOpenGlContextManager::set_vsync(const tbx::VsyncMode& mode)
    {
        _settings.vsync_mode = mode;
        apply_vsync_setting();

        auto result = tbx::Result {};
        result.flag_success();
        return result;
    }

    tbx::GraphicsProcAddress SdlOpenGlContextManager::get_proc_address() const
    {
        return reinterpret_cast<tbx::GraphicsProcAddress>(SDL_GL_GetProcAddress);
    }

    void SdlOpenGlContextManager::set_window(tbx::Window window, SDL_Window* sdl_window)
    {
        if (!window.is_valid())
            return;

        const auto previous_it = _native_windows.find(window);
        if (previous_it != _native_windows.end() && previous_it->second != sdl_window)
            destroy_context(previous_it->second);

        if (!sdl_window)
        {
            _native_windows.erase(window);
            return;
        }

        _native_windows[window] = sdl_window;
    }

    void SdlOpenGlContextManager::remove_window(
        const tbx::Window& window,
        SDL_Window* previous_sdl_window)
    {
        if (!previous_sdl_window)
        {
            const auto window_it = _native_windows.find(window);
            if (window_it != _native_windows.end())
                previous_sdl_window = window_it->second;
        }

        if (previous_sdl_window)
            destroy_context(previous_sdl_window);

        _native_windows.erase(window);
    }

    void SdlOpenGlContextManager::apply_default_attributes() const
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

    int SdlOpenGlContextManager::get_swap_interval() const
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

    tbx::Result SdlOpenGlContextManager::make_failure(std::string message) const
    {
        auto result = tbx::Result {};
        result.flag_failure(std::move(message));
        return result;
    }

    bool SdlOpenGlContextManager::try_create_context(
        SDL_Window* sdl_window,
        const std::string& window_title)
    {
        if (!sdl_window)
            return false;

        if (_contexts.contains(sdl_window))
            return try_make_current(sdl_window, window_title);

        SDL_GLContext context = SDL_GL_CreateContext(sdl_window);
        if (!context)
        {
            TBX_TRACE_ERROR(
                "Failed to create SDL OpenGL context for window '{}': {}",
                window_title,
                SDL_GetError());
            return false;
        }

        _contexts[sdl_window] = context;
        if (!SDL_GL_MakeCurrent(sdl_window, context))
        {
            TBX_TRACE_ERROR(
                "Failed to make SDL OpenGL context current for window '{}': {}",
                window_title,
                SDL_GetError());
            destroy_context(sdl_window);
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

    void SdlOpenGlContextManager::destroy_context(SDL_Window* sdl_window)
    {
        if (!sdl_window)
            return;

        auto context_it = _contexts.find(sdl_window);
        if (context_it == _contexts.end())
            return;

        if (context_it->second)
        {
            try_release_current_context(context_it->second);
            SDL_GL_DestroyContext(context_it->second);
        }
        _contexts.erase(context_it);
    }

    bool SdlOpenGlContextManager::try_make_current(
        SDL_Window* sdl_window,
        const std::string& window_title)
    {
        if (!sdl_window)
            return false;

        auto context_it = _contexts.find(sdl_window);
        if (context_it == _contexts.end())
            return false;

        if (!SDL_GL_MakeCurrent(sdl_window, context_it->second))
        {
            TBX_TRACE_ERROR(
                "Failed to make OpenGL context current for window '{}': {}",
                window_title,
                SDL_GetError());
            return false;
        }

        return true;
    }

    bool SdlOpenGlContextManager::try_present(SDL_Window* sdl_window)
    {
        if (!sdl_window || !_contexts.contains(sdl_window))
            return false;

        SDL_GL_SwapWindow(sdl_window);
        return true;
    }

    void SdlOpenGlContextManager::apply_vsync_setting()
    {
        SDL_Window* current_window = SDL_GL_GetCurrentWindow();
        SDL_GLContext current_context = SDL_GL_GetCurrentContext();

        for (const auto& [sdl_window, context] : _contexts)
        {
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
