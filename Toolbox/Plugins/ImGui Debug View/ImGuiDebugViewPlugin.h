#pragma once
#include <Tbx/Core/Plugins/RegisterPlugin.h>
#include <Tbx/App/Layers/Layer.h>

namespace ImGuiDebugViewPlugin
{
    class ImGuiDebugViewPlugin : public Tbx::Plugin<Tbx::Layer>
    {
    public:
        Tbx::Layer* Provide() override;
        void Destroy(Tbx::Layer* toDestroy) override;
        void OnLoad() override;
        void OnUnload() override;
    };
}