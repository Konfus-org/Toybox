#pragma once
#include "GLFWInputHandler.h"
#include <Tbx/App/Input/IInputHandler.h>
#include <Tbx/Core/Plugins/PluginsAPI.h>

namespace GLFWInput
{
    class GLFWInputHandlerPlugin : public GLFWInputHandler, public Tbx::Plugin
    {
    public:
        void OnLoad() override;
        void OnUnload() override;
    };
}
