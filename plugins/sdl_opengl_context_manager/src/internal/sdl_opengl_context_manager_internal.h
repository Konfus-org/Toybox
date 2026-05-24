#pragma once
#include "tbx/interfaces/opengl_context_backend.h"
#include "tbx/plugins/sdl_opengl_context_manager/sdl_opengl_context_manager.h"
#include "tbx/systems/debugging/macros.h"
#include <string_view>
#include <utility>

namespace sdl_opengl_context_manager::internal
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

}
