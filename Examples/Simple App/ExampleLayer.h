#pragma once
#include <Tbx/Layers/Layer.h>
#include <Tbx/TBS/Box.h>
#include <Tbx/TBS/Toy.h>
#include <Tbx/Graphics/Material.h>

class ExampleLayer : public Tbx::Layer
{
public:
    using Layer::Layer;

    bool IsOverlay() override { return false; }
    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate() override;

private:
    std::shared_ptr<Tbx::Box> _level = nullptr;

    Tbx::ToyHandle _fpsCam = {};
    Tbx::ToyHandle _smily = {};

    float _smilyBobTime = 0.0f;
    float _smilyBobAmplitude = 0.0f;

    float _camPitch = 0.0f;
    float _camYaw = 0.0f;
};

