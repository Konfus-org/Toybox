#pragma once
#include <Tbx/Layers/Layer.h>
#include <Tbx/TBS/Box.h>
#include <Tbx/TBS/Toy.h>

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
    Tbx::Toy _fpsCam = {};
    float _camPitch = 0.0f;
    float _camYaw = 0.0f;
    float _camMoveSpeed = 1.0f;
    float _camRotateSpeed = 180.0f;
};

