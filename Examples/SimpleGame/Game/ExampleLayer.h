#pragma once
#include <Tbx/Layers/Layer.h>
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
    std::shared_ptr<Tbx::Toy> _fpsCam = nullptr;
    std::shared_ptr<Tbx::Toy> _smily = nullptr;

    float _smilyBobTime = 0.0f;
    float _smilyBobAmplitude = 0.0f;

    float _camPitch = 0.0f;
    float _camYaw = 0.0f;
};

