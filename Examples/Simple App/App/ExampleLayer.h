#pragma once
#include <Tbx/Runtime/Layers/Layer.h>
#include <Tbx/Core/TBS/PlaySpace.h>
#include <Tbx/Core/TBS/Toy.h>

#include "Tbx/Core/Math/Vectors.h"

class ExampleLayer : public Tbx::Layer
{
public:
    using Layer::Layer;

    bool IsOverlay() override { return false; }
    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate() override;

private:
    std::shared_ptr<Tbx::PlaySpace> _level = nullptr;
    Tbx::Toy _fpsCam = {};
    float _camPitch = 0.0f;
    float _camYaw = 0.0f;
    float _camMoveSpeed = 1.0f;
    float _camRotateSpeed = 180.0f;
};

