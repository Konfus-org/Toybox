#pragma once
#include <Tbx/Plugins/Plugin.h>
#include <Tbx/Gui/ImmediateGui.h>

namespace ImGuiDebugView
{
    class ImGuiDebugViewPlugin final
        : public Tbx::IImmediateGui
        , public Tbx::Plugin
    {
    public:
        ImGuiDebugViewPlugin(Tbx::Ref<Tbx::EventBus> eventBus);
        ~ImGuiDebugViewPlugin() override;

        /*bool BeginWindow(const std::string& title, bool* open) override;
        void EndWindow() override;
        void Text(const std::string& text) override;
        bool Button(const std::string& label, const Tbx::Size& size) override;
        bool Checkbox(const std::string& label, bool* value) override;
        bool SliderFloat(const std::string& label, float* value, float min, float max) override;
        bool InputText(const std::string& label, char* buffer, size_t bufferSize) override;
        void SameLine() override;
        void Separator() override;*/
    };

    TBX_REGISTER_PLUGIN(ImGuiDebugViewPlugin);
}