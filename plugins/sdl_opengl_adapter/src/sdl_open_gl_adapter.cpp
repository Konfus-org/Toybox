#include "tbx/plugins/sdl_opengl_adapter/sdl_open_gl_adapter.h"
#include "tbx/debugging/macros.h"

namespace tbx::plugins
{
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

    SdlOpenGlAdapter::SdlOpenGlAdapter(const SdlOpenGlAdapterSettings& settings)
        : _settings(settings)
    {
    }

    SdlOpenGlAdapter::~SdlOpenGlAdapter() noexcept
    {
        for (const auto& context_entry : _contexts)
        {
            if (context_entry.second)
                SDL_GL_DestroyContext(context_entry.second);
        }
        _contexts.clear();
    }

    void SdlOpenGlAdapter::set_settings(const SdlOpenGlAdapterSettings& settings)
    {
        _settings = settings;
    }

    void SdlOpenGlAdapter::apply_default_attributes() const
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

    bool SdlOpenGlAdapter::try_create_context(
        SDL_Window* sdl_window,
        const std::string& window_title)
    {
        if (!sdl_window)
            return false;

        if (_contexts.contains(sdl_window))
        {
            if (!try_make_current(sdl_window, window_title))
                return false;
            return true;
        }

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

        int interval = _settings.is_vsync_enabled ? 1 : 0;
        SDL_ClearError();
        if (!SDL_GL_SetSwapInterval(interval))
        {
            TBX_TRACE_WARNING(
                "Failed to set vsync={} for window '{}': {}",
                _settings.is_vsync_enabled,
                window_title,
                SDL_GetError());
            SDL_ClearError();
        }

        return true;
    }

    void SdlOpenGlAdapter::destroy_context(SDL_Window* sdl_window)
    {
        if (!sdl_window)
            return;

        auto context_it = _contexts.find(sdl_window);
        if (context_it == _contexts.end())
            return;

        if (context_it->second)
            SDL_GL_DestroyContext(context_it->second);
        _contexts.erase(context_it);
    }

    bool SdlOpenGlAdapter::try_make_current(SDL_Window* sdl_window, const std::string& window_title)
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

    bool SdlOpenGlAdapter::try_present(SDL_Window* sdl_window)
    {
        if (!sdl_window || !_contexts.contains(sdl_window))
            return false;

        SDL_GL_SwapWindow(sdl_window);
        return true;
    }

    void SdlOpenGlAdapter::apply_vsync_setting(const std::vector<SDL_Window*>& windows)
    {
        SDL_Window* current_window = SDL_GL_GetCurrentWindow();
        SDL_GLContext current_context = SDL_GL_GetCurrentContext();

        int interval = _settings.is_vsync_enabled ? 1 : 0;
        for (SDL_Window* sdl_window : windows)
        {
            if (!sdl_window)
                continue;
            auto context_it = _contexts.find(sdl_window);
            if (context_it == _contexts.end())
                continue;

            if (!SDL_GL_MakeCurrent(sdl_window, context_it->second))
            {
                TBX_TRACE_WARNING(
                    "Failed to make OpenGL context current to apply vsync: {}",
                    SDL_GetError());
                continue;
            }

            SDL_ClearError();
            if (SDL_GL_SetSwapInterval(interval) != 0)
            {
                TBX_TRACE_WARNING(
                    "Failed to set vsync={}: {}",
                    _settings.is_vsync_enabled,
                    SDL_GetError());
                SDL_ClearError();
            }
        }

        if (current_window && current_context)
        {
            SDL_GL_MakeCurrent(current_window, current_context);
        }
    }

    GraphicsProcAddress SdlOpenGlAdapter::get_proc_address() const
    {
        return reinterpret_cast<GraphicsProcAddress>(SDL_GL_GetProcAddress);
    }

    bool SdlOpenGlAdapter::has_context(SDL_Window* sdl_window) const
    {
        return sdl_window != nullptr && _contexts.contains(sdl_window);
    }
}
