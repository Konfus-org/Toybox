#pragma once
#include <Tbx/Application/Layers/Layer.h>
#include <Tbx/Systems/TBS/PlaySpace.h>
#include <Tbx/Systems/TBS/Toy.h>

class ExampleLayer : public Tbx::Layer
{
public:
    using Layer::Layer;

    bool IsOverlay() override { return false; }
    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate() override;

private:
    std::shared_ptr<Tbx::Playspace> _level = nullptr;
    Tbx::Toy _fpsCam = {};
    float _camPitch = 0.0f;
    float _camYaw = 0.0f;
    float _camMoveSpeed = 1.0f;
    float _camRotateSpeed = 180.0f;
};

