#pragma once
#include <Toybox.h>

namespace Tbx::Editor
{
    class EditorLayer : public Tbx::Layer
    {
    public:
        EditorLayer(const std::string& name) : Tbx::Layer(name) { }

        void OnAttach() override;
        void OnDetach() override;
        void OnUpdate() override;
    };
}

