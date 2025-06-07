#include "ImGuiDebugViewPlugin.h"
#include <imgui.h>

namespace ImGuiDebugView
{
    void ImGuiDebugViewPlugin::OnLoad()
    {
        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags = io.ConfigFlags | ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        io.ConfigFlags = io.ConfigFlags | ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    }
    
    void ImGuiDebugViewPlugin::OnUnload()
    {
        ImGui::DestroyContext();
    }
}