#include "ImGuiDebugViewPlugin.h"

namespace ImGuiDebugViewPlugin
{
    Tbx::Layer* ImGuiDebugViewPlugin::Provide()
    {
        return nullptr;
    }

    void ImGuiDebugViewPlugin::Destroy(Tbx::Layer* toDestroy)
    {
        delete toDestroy;
    }

    void ImGuiDebugViewPlugin::OnLoad()
    {
    }
    
    void ImGuiDebugViewPlugin::OnUnload()
    {

    }
}