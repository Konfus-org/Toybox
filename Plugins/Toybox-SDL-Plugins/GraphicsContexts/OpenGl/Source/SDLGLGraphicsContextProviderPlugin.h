#pragma
#include "SDLGLGraphicsContext.h"
#include "Tbx/Plugins/Plugin.h"
#include "Tbx/Graphics/GraphicsContext.h"
#include "Tbx/Windowing/Window.h"

namespace Tbx::Plugins::SDLGraphicsContext
{
    class SDLOpenGlGraphicsContextsProviderPlugin final
        : public FactoryPlugin<SDLGLGraphicsContext>
        , public IGraphicsContextProvider
    {
    public:
        SDLOpenGlGraphicsContextsProviderPlugin(Ref<EventBus> eventBus) {}
        GraphicsApi GetApi() const override;
        Ref<IGraphicsContext> Provide(const Window* window) override;
    };

    TBX_REGISTER_PLUGIN(SDLOpenGlGraphicsContextsProviderPlugin);
}
