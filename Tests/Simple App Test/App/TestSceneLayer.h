#pragma once
#include <Tbx/Core/TBS/Playspace.h>
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
    std::shared_ptr<Tbx::Playspace> _playSpace;

    float _camMoveSpeed = 1.0f;
    float _camRotateSpeed = 180.0f;
};

