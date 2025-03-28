#pragma once
#include <App/App.h>

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
        Tbx::GUID _openNewWindowRequestEventId;
        std::shared_ptr<Tbx::IWindowFactory> _windowFactory;

        void OnOpenNewWindow(Tbx::OpenNewWindowRequestEvent& e);
    };
}
