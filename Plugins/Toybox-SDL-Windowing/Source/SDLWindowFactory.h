#pragma once
#include <Tbx/Plugin API/RegisterPlugin.h>
#include <Tbx/Events/WindowEvents.h>

namespace SDLWindowing
{
    class SDLWindowFactory : public Tbx::IWindowFactoryPlugin
    {
    public:
        void OnLoad() override;
        void OnUnload() override;

    protected:
        Tbx::IWindow* New() override;
        void Delete(Tbx::IWindow* window) override;
    };

    TBX_REGISTER_PLUGIN(SDLWindowFactory);
}
