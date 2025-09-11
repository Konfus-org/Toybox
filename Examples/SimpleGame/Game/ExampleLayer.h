#pragma once
#include <Tbx/ECS/Toy.h>
#include <Tbx/ECS/ThreeDSpace.h>
#include <Tbx/Graphics/Material.h>

class ExampleLayer : public Tbx::Layer
{
public:
    ExampleLayer(const std::string& name, std::weak_ptr<Tbx::App> app)
        : Layer(name, app) {}

    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate() override;

private:
    std::shared_ptr<Tbx::ThreeDSpace> _world = nullptr;
    std::shared_ptr<Tbx::Toy> _fpsCam = nullptr;
    std::shared_ptr<Tbx::Toy> _smily = nullptr;

    float _smilyBobTime = 0.0f;
    float _smilyBobAmplitude = 0.0f;

    float _camPitch = 0.0f;
    float _camYaw = 0.0f;

    Tbx::Material _simpleTexturedMat;
};

