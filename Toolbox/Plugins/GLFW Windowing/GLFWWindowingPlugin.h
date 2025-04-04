#pragma once
#include "GLFWWindowFactory.h"
#include <Tbx/Core/Plugins/RegisterPlugin.h>
#include <Tbx/App/Events/WindowEvents.h>
#include <Tbx/App/Windowing/IWindowFactory.h>

namespace GLFWWindowing
{
    class GLFWWindowingPlugin : public GLFWWindowFactory, public Tbx::Plugin
    {
    public:
        void OnLoad() override;
        void OnUnload() override;

    private:
        Tbx::UID _openNewWindowRequestEventId;
        std::shared_ptr<Tbx::IWindowFactory> _windowFactory;

        void OnOpenNewWindow(Tbx::OpenNewWindowRequestEvent& e);
    };
}
