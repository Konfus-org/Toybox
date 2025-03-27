#pragma once
#include <Toybox.h>

class TestLayer : public Tbx::Layer
{
public:
    using Layer::Layer;

    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate() override;
};

