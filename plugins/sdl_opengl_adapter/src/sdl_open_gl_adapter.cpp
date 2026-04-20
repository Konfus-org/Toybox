#include "tbx/plugins/sdl_opengl_adapter/sdl_open_gl_adapter.h"
#include "tbx/debugging/macros.h"
#include <string>
#include <utility>

namespace sdl_opengl_adapter
{
    namespace
    {
        void try_release_current_context(SDL_GLContext context)
        {
            if (!context)
                return;

            const auto current_context = SDL_GL_GetCurrentContext();
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

        void set_opengl_attribute(SDL_GLAttr attribute, int value)
        {
            if (!SDL_GL_SetAttribute(attribute, value))
            {
                TBX_TRACE_WARNING(
                    "Failed to set SDL OpenGL attribute {}: {}",
                    static_cast<int>(attribute),
                    SDL_GetError());
                SDL_ClearError();
            }
        }
    }

    SdlOpenGlAdapter::SdlOpenGlAdapter(
        tbx::IWindowManager& window_manager,
        const SdlOpenGlAdapterSettings& settings)
        : _window_manager(window_manager)
        , _settings(settings)
    {
    }

    SdlOpenGlAdapter::~SdlOpenGlAdapter() noexcept
    {
        release_all_contexts();
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

    void SdlOpenGlAdapter::sync_window(const tbx::Window& window, SDL_Window* native_window)
    {
        if (!window.is_valid())
            return;

        if (!native_window)
        {
            release_window(window);
            return;
        }

        auto& record = _contexts[window];
        record.window = window;
        if (record.native_window == native_window)
            return;

        release_record(record);
        record.native_window = native_window;
    }

    void SdlOpenGlAdapter::release_window(const tbx::Window& window)
    {
        const auto record_it = _contexts.find(window);
        if (record_it == _contexts.end())
            return;

        release_record(record_it->second);
        _contexts.erase(record_it);
    }

    tbx::Result SdlOpenGlAdapter::make_current(const tbx::Window& window)
    {
        SDL_Window* native_window = get_native_window(window);
        if (!native_window)
            return tbx::Result(false, "SDL OpenGL adapter: native window not available.");

        auto& record = _contexts[window];
        record.window = window;
        if (record.native_window != native_window)
        {
            release_record(record);
            record.native_window = native_window;
        }

        if (const auto ensure_result = ensure_context(window, native_window, record); !ensure_result)
            return ensure_result;

        return set_current_context(record);
    }

    tbx::Result SdlOpenGlAdapter::present(const tbx::Window& window)
    {
        const auto record_it = _contexts.find(window);
        if (record_it == _contexts.end() || !record_it->second.native_window || !record_it->second.context)
        {
            return tbx::Result(false, "SDL OpenGL adapter: context not available.");
        }

        SDL_GL_SwapWindow(record_it->second.native_window);
        return {};
    }

    tbx::Result SdlOpenGlAdapter::set_vsync(const tbx::VsyncMode& mode)
    {
        _vsync_mode = mode;

        for (auto& [window, record] : _contexts)
        {
            (void)window;
            if (!record.native_window || !record.context)
                continue;

            if (const auto set_current_result = set_current_context(record); !set_current_result)
                return set_current_result;

            if (const auto apply_result = try_apply_vsync(record); !apply_result)
                return apply_result;
        }

        return {};
    }

    tbx::GraphicsProcAddress SdlOpenGlAdapter::get_proc_address() const
    {
        return reinterpret_cast<tbx::GraphicsProcAddress>(SDL_GL_GetProcAddress);
    }

    tbx::Result SdlOpenGlAdapter::ensure_context(
        const tbx::Window& window,
        SDL_Window* native_window,
        SdlOpenGlContextRecord& record)
    {
        if (record.context)
            return {};

        apply_default_attributes();
        record.context = SDL_GL_CreateContext(native_window);
        if (!record.context)
        {
            auto report = std::string("SDL OpenGL adapter: failed to create context for window ");
            report += tbx::to_string(window);
            report += ": ";
            report += SDL_GetError();
            SDL_ClearError();
            return tbx::Result(false, std::move(report));
        }

        if (const auto make_current_result = set_current_context(record); !make_current_result)
        {
            release_record(record);
            return make_current_result;
        }

        if (const auto vsync_result = try_apply_vsync(record); !vsync_result)
            return vsync_result;

        if (!SDL_GL_MakeCurrent(nullptr, nullptr))
        {
            TBX_TRACE_WARNING(
                "Failed to release SDL OpenGL context for window '{}': {}",
                tbx::to_string(window),
                SDL_GetError());
            SDL_ClearError();
        }

        return {};
    }

    SDL_Window* SdlOpenGlAdapter::get_native_window(const tbx::Window& window) const
    {
        if (!window.is_valid() || !_window_manager.is_open(window))
            return nullptr;

        return static_cast<SDL_Window*>(_window_manager.get_native_handle(window));
    }

    int SdlOpenGlAdapter::get_swap_interval() const
    {
        switch (_vsync_mode)
        {
            case tbx::VsyncMode::OFF:
                return 0;
            case tbx::VsyncMode::ON:
                return 1;
            case tbx::VsyncMode::ADAPTIVE:
                return -1;
        }

        return 0;
    }

    void SdlOpenGlAdapter::release_all_contexts()
    {
        for (auto& [window, record] : _contexts)
        {
            (void)window;
            release_record(record);
        }

        _contexts.clear();
    }

    void SdlOpenGlAdapter::release_record(SdlOpenGlContextRecord& record)
    {
        if (record.context)
        {
            try_release_current_context(record.context);
            SDL_GL_DestroyContext(record.context);
            record.context = nullptr;
        }

        record.native_window = nullptr;
    }

    tbx::Result SdlOpenGlAdapter::set_current_context(SdlOpenGlContextRecord& record) const
    {
        if (!record.native_window || !record.context)
            return tbx::Result(false, "SDL OpenGL adapter: context not available.");

        if (!SDL_GL_MakeCurrent(record.native_window, record.context))
        {
            auto report = std::string("SDL OpenGL adapter: failed to make context current for window ");
            report += tbx::to_string(record.window);
            report += ": ";
            report += SDL_GetError();
            SDL_ClearError();
            return tbx::Result(false, std::move(report));
        }

        return {};
    }

    tbx::Result SdlOpenGlAdapter::try_apply_vsync(SdlOpenGlContextRecord& record) const
    {
        SDL_ClearError();
        if (!SDL_GL_SetSwapInterval(get_swap_interval()))
        {
            auto report = std::string("SDL OpenGL adapter: failed to set vsync for window ");
            report += tbx::to_string(record.window);
            report += ": ";
            report += SDL_GetError();
            SDL_ClearError();
            return tbx::Result(false, std::move(report));
        }

        return {};
    }
}
