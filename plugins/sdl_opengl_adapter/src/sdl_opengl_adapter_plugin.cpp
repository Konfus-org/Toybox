#include "sdl_opengl_adapter_plugin.h"
#include "tbx/debugging/macros.h"
#include "tbx/graphics/messages.h"
#include "tbx/messages/observable.h"
#include <SDL3/SDL.h>
#include <glad/glad.h>

namespace tbx::plugins
{
    static void set_opengl_attribute(SDL_GLAttr attribute, int value)
    {
        if (SDL_GL_SetAttribute(attribute, value) != 0)
        {
            TBX_TRACE_WARNING(
                "Failed to set SDL OpenGL attribute {}: {}",
                static_cast<int>(attribute),
                SDL_GetError());
        }
    }


    bool SdlOpenGlAdapterPlugin::try_load_glad_with_sdl() const
    {
        if (SDL_GL_GetCurrentContext() == nullptr)
        {
            return false;
        }

        const auto loaded = gladLoadGLLoader(SDL_GL_GetProcAddress);
        if (!loaded)
        {
            TBX_TRACE_ERROR(
                "SDL OpenGL adapter: failed to load OpenGL entry points via SDL.");
            return false;
        }

        TBX_TRACE_INFO("SDL OpenGL adapter loaded OpenGL entry points via SDL.");
        return true;
    }

    void SdlOpenGlAdapterPlugin::configure_opengl_attributes() const
    {
        if ((SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) == 0)
        {
            TBX_TRACE_WARNING(
                "SDL OpenGL adapter: SDL video subsystem is not initialized; OpenGL "
                "attributes were not configured.");
            return;
        }

        set_opengl_attribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        set_opengl_attribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
        set_opengl_attribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        set_opengl_attribute(SDL_GL_DEPTH_SIZE, 24);
        set_opengl_attribute(SDL_GL_STENCIL_SIZE, 8);
        set_opengl_attribute(SDL_GL_DOUBLEBUFFER, 1);
#if defined(TBX_DEBUG)
        set_opengl_attribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif
    }

    void SdlOpenGlAdapterPlugin::on_attach(Application&)
    {
        configure_opengl_attributes();
        _needs_glad_load = true;
    }

    void SdlOpenGlAdapterPlugin::on_detach()
    {
        _glad_loaded = false;
        _needs_glad_load = true;
    }

    void SdlOpenGlAdapterPlugin::on_recieve_message(Message& msg)
    {
        if (on_message(
                msg,
                [this](WindowContextReadyEvent& event)
                {
                    handle_window_ready(event);
                }))
        {
            return;
        }
    }

    void SdlOpenGlAdapterPlugin::handle_window_ready(WindowContextReadyEvent&)
    {
        if (!_needs_glad_load || _glad_loaded)
        {
            return;
        }

        _glad_loaded = try_load_glad_with_sdl();
        _needs_glad_load = !_glad_loaded;
    }
}
