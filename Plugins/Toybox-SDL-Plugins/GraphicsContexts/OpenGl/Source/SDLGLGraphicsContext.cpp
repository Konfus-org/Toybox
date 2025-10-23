#include "SDLGLGraphicsContext.h"
#include "Tbx/Debug/Asserts.h"
#include <glad/glad.h>
#include <SDL3/SDL_hints.h>

namespace Tbx::Plugins::SDLGraphicsContext
{
    SDLGLGraphicsContext::SDLGLGraphicsContext(SDL_Window* window)
        : _window(window)
    {
        TBX_ASSERT(window, "SDLGLContext: Invalid window given!");

        // Set attribute
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#ifdef TBX_DEBUG
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif
        // Validate attributes
        int att = 0;
        SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &att);
        TBX_ASSERT(att == 4, "SDLGLContext: Failed to set OpenGL context major version to 4");
        SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &att);
        TBX_ASSERT(att == 5, "SDLGLContext: Failed to set OpenGL context minor version to 5");
        SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &att);
        TBX_ASSERT(att == SDL_GL_CONTEXT_PROFILE_CORE, "SDLGLContext: Failed to set OpenGL context profile to core");
#ifdef TBX_DEBUG
        SDL_GL_GetAttribute(SDL_GL_CONTEXT_FLAGS, &att);
        TBX_ASSERT(att == SDL_GL_CONTEXT_DEBUG_FLAG, "SDLGLContext: Failed to set OpenGL context debug flag");
#endif

        _glContext = SDL_GL_CreateContext(_window);
        TBX_ASSERT(_glContext, "SDLGLContext: Failed to create gl context for window!");
        SDL_GL_MakeCurrent(_window, _glContext);
        int gladStatus = gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress);
        TBX_ASSERT(gladStatus, "GL Rendering: Failed to initialize Glad!");
    }

    SDLGLGraphicsContext::~SDLGLGraphicsContext()
    {
        SDL_GL_DestroyContext(_glContext);
        _glContext = nullptr;
    }

    void SDLGLGraphicsContext::MakeCurrent()
    {
        SDL_GL_MakeCurrent(_window, _glContext);
    }

    void SDLGLGraphicsContext::Present()
    {
        SDL_GL_SwapWindow(_window);
    }

    void SDLGLGraphicsContext::SetVsync(Tbx::VsyncMode mode)
    {
        int interval = 0;
        switch (mode)
        {
            case VsyncMode::Off:
                interval = 0;
                break;
            case VsyncMode::On:
                interval = 1;
                break;
            case VsyncMode::Adaptive:
                interval = -1;
                break;
        }
        SDL_GL_SetSwapInterval(interval);
    }
}