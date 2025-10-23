#include "ImGuiDebugViewPlugin.h"
#include <imgui.h>

namespace ImGuiDebugView
{
    ImGuiDebugViewPlugin::ImGuiDebugViewPlugin(Tbx::Ref<Tbx::EventBus> eventBus)
    {
        // TODO Setup Dear ImGui
    }

    ImGuiDebugViewPlugin::~ImGuiDebugViewPlugin()
    {
        //ImGui::DestroyContext();
    }

   /* bool ImGuiDebugViewPlugin::BeginWindow(const std::string& title, bool* open)
    {
        return false;
    }

    void ImGuiDebugViewPlugin::EndWindow()
    {
    }

    void ImGuiDebugViewPlugin::Text(const std::string& text)
    {
    }

    bool ImGuiDebugViewPlugin::Button(const std::string& label, const Tbx::Size& size)
    {
        return false;
    }

    bool ImGuiDebugViewPlugin::Checkbox(const std::string& label, bool* value)
    {
        return false;
    }

    bool ImGuiDebugViewPlugin::SliderFloat(const std::string& label, float* value, float min, float max)
    {
        return false;
    }

    bool ImGuiDebugViewPlugin::InputText(const std::string& label, char* buffer, size_t bufferSize)
    {
        return false;
    }

    void ImGuiDebugViewPlugin::SameLine()
    {
    }

    void ImGuiDebugViewPlugin::Separator()
    {
    }*/
}
