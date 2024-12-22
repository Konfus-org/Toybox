#pragma once
#include <Toybox.h>

class TestLayer : public Toybox::Layer
{
public:
    using Layer::Layer;

    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate() override;
    void OnEvent(Toybox::Event& event) override;
};

