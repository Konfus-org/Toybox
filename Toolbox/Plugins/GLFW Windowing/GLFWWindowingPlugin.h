#pragma once
#include "GLFWWindowFactory.h"
#include <Tbx/Core/Plugins/RegisterPlugin.h>
#include <Tbx/App/Events/WindowEvents.h>
#include <Tbx/App/Windowing/IWindowFactory.h>

namespace GLFWWindowing
{
    class GLFWWindowingPlugin : public Tbx::Plugin, public GLFWWindowFactory
    {
    public:
        GLFWWindowingPlugin() = default;
        ~GLFWWindowingPlugin() final = default;

        void OnLoad() override;
        void OnUnload() override;

    private:
        Tbx::UID _openNewWindowRequestEventId;
        std::shared_ptr<Tbx::IWindowFactory> _windowFactory;

        void OnOpenNewWindow(Tbx::OpenNewWindowRequest& e);
    };
}

TBX_REGISTER_PLUGIN(GLFWWindowing::GLFWWindowingPlugin);
