#pragma once
#include <Tbx/Core/Plugins/RegisterPlugin.h>
#include <Tbx/App/Layers/Layer.h>

namespace ImGuiDebugViewPlugin
{
    class ImGuiDebugViewPlugin : public Tbx::Layer, public Tbx::Plugin
    {
    public:
        void OnLoad() override;
        void OnUnload() override;
    };
}