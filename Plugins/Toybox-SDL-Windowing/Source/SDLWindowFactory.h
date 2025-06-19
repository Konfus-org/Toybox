#pragma once
#include <Tbx/Plugin API/PluginInterfaces.h>

namespace SDLWindowing
{
    class SDLWindowFactory : public Tbx::IWindowFactoryPlugin
    {
    public:
        void OnLoad() override;
        void OnUnload() override;

        std::shared_ptr<Tbx::IWindow> Create(const std::string& title, const Tbx::Size& size) override;
    };
}
