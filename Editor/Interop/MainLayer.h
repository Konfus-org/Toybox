#pragma once
#include <Toybox.h>

namespace Toybox::Interop
{
    class MainLayer : public Toybox::Layers::Layer
    {
    public:
        MainLayer(const std::string& name) : Toybox::Layers::Layer(name) { }

        void OnAttach() override;
        void OnDetach() override;
        void OnUpdate() override;
        void OnEvent(Toybox::Events::Event& event) override;
    };
}

