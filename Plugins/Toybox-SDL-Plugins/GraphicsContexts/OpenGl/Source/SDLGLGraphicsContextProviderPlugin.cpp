#include "Tbx/Debug/Asserts.h"
#include "SDLGLGraphicsContextProviderPlugin.h"
#include "SDLGLGraphicsContext.h"

namespace Tbx::Plugins::SDLGraphicsContext
{
   Ref<Tbx::IGraphicsContext> SDLOpenGlGraphicsContextsProviderPlugin::Provide(const Window* window)
    {
        if (!window)
        {
            TBX_ASSERT(false, "SDL OpenGL Graphics Context Provider: Given null window to provide context for.");
            return nullptr;
        }
        auto* sdlWindow = std::any_cast<SDL_Window*>(window->GetNativeWindow());
        return Create(sdlWindow);
    }

    GraphicsApi SDLOpenGlGraphicsContextsProviderPlugin::GetApi() const
    {
        return GraphicsApi::OpenGL;
    }
}
