#pragma once
#include <Toybox.h>

class TestLayer : public Toybox::Layers::Layer
{
public:
    TestLayer(const std::string& name) : Layer(name) { }

    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate() override;
    void OnEvent(Toybox::Events::Event& event) override;
};

