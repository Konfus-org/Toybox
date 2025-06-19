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

        std::shared_ptr<Tbx::IWindow> Create(const std::string& title, const Tbx::Size& size) override;

    private:
        void OnCreateWindowRequest(Tbx::CreateWindowRequest& r);

        Tbx::UID _openCreateWindowRequestId = -1;
    };

    TBX_REGISTER_PLUGIN(SDLWindowFactory);
}
