#pragma once
#include <Tbx/Core/Plugins/RegisterPlugin.h>
#include <Tbx/App/Events/WindowEvents.h>
#include <Tbx/App/Windowing/IWindowFactory.h>

namespace GLFWWindowing
{
    class GLFWWindowingPlugin : public Tbx::Plugin<Tbx::IWindowFactory>
    {
    public:
        Tbx::IWindowFactory* Provide() override;
        void Destroy(Tbx::IWindowFactory* toDestroy) override;
        void OnLoad() override;
        void OnUnload() override;

    private:
        Tbx::UID _openNewWindowRequestEventId;
        std::shared_ptr<Tbx::IWindowFactory> _windowFactory;

        void OnOpenNewWindow(Tbx::OpenNewWindowRequestEvent& e);
    };
}
