#pragma once
#include <Tbx/Core/TBS/PlaySpace.h>
#include <Tbx/Runtime/Layers/Layer.h>
#include <memory>

class TestSceneLayer : public Tbx::Layer
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

