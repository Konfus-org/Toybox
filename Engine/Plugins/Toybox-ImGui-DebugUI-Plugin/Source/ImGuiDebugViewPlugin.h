#pragma once
#include "ImGuiDebugViewLayer.h"
#include <Tbx/Core/Plugins/RegisterPlugin.h>

namespace ImGuiDebugView
{
    class ImGuiDebugViewPlugin : public Tbx::Plugin, public ImGuiDebugViewLayer
    {
    public:
        ImGuiDebugViewPlugin() = default;
        ~ImGuiDebugViewPlugin() final = default;

        void OnLoad() override;
        void OnUnload() override;
    };

    TBX_REGISTER_PLUGIN(ImGuiDebugViewPlugin)
}