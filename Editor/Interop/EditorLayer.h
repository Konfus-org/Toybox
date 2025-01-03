#pragma once
#include <Tbx.h>

namespace Tbx::Editor
{
    class EditorLayer : public Tbx::Layer
    {
    public:
        EditorLayer(const std::string& name) : Tbx::Layer(name) { }

        void OnAttach() override;
        void OnDetach() override;
        void OnUpdate() override;
        void OnEvent(Tbx::Event& event) override;
    };
}

