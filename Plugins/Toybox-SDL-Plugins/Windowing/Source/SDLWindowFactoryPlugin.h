#pragma 
#include "SDLWindow.h"
#include "Tbx/Plugins/Plugin.h"
#include "Tbx/Events/AppEvents.h"
#include "Tbx/Events/EventListener.h"

namespace Tbx::Plugins::SDLWindowing
{
    class SDLWindowFactoryPlugin final
        : public FactoryPlugin<SDLWindow>
        , public IWindowFactory
    {
    public:
        SDLWindowFactoryPlugin(Ref<EventBus> eventBus);
        ~SDLWindowFactoryPlugin() override;

        Ref<Window> Create(
            const std::string& title,
            const Size& size,
            const WindowMode& mode,
            Ref<EventBus> eventBus) override;

    private:
        void OnAppSettingsChanged(const AppSettingsChangedEvent& e);

    private:
        EventListener _listener = {};
        bool _usingOpenGl = false;
    };

    TBX_REGISTER_PLUGIN(SDLWindowFactoryPlugin);
}
