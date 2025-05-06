#pragma once
#include <Tbx/Runtime/Layers/Layer.h>

class ExampleLayer : public Tbx::Layer
{
public:
    using Layer::Layer;

    bool IsOverlay() override { return false; }
    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate() override;

private:
    float _camMoveSpeed = 1.0f;
    float _camRotateSpeed = 180.0f;
};

