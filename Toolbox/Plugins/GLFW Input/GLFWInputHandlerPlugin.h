#pragma once
#include "GLFWInputHandler.h"
#include <Tbx/App/Input/IInputHandler.h>
#include <Tbx/Core/Plugins/PluginsAPI.h>

namespace GLFWInput
{
    class GLFWInputHandlerPlugin : public Tbx::Plugin, public GLFWInputHandler
    {
    public:
        GLFWInputHandlerPlugin() = default;
        ~GLFWInputHandlerPlugin() final = default;

        void OnLoad() override;
        void OnUnload() override;
    };
}
