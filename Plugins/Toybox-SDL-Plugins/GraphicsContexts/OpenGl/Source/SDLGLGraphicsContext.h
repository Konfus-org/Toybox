#pragma once
#include "Tbx/Graphics/GraphicsContext.h"
#include "Tbx/Plugins/Plugin.h"
#include <SDL3/SDL_video.h>

namespace Tbx::Plugins::SDLGraphicsContext
{
    class SDLGLGraphicsContext final : public IGraphicsContext, public IProductOfPluginFactory
    {
    public:
        SDLGLGraphicsContext(SDL_Window* window);
        ~SDLGLGraphicsContext();

        void MakeCurrent() override;

        void Present() override;

        void SetVsync(VsyncMode mode) override;

    private:
        SDL_Window* _window = nullptr;
        SDL_GLContext _glContext = nullptr;
    };
}

