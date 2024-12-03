#pragma once
#include <Toybox.h>

namespace Toybox::Editor
{
    class EditorLayer : public Toybox::Layer
    {
    public:
        EditorLayer(const std::string& name) : Toybox::Layer(name) { }

        void OnAttach() override;
        void OnDetach() override;
        void OnUpdate() override;
        void OnEvent(Toybox::Event& event) override;
    };
}

