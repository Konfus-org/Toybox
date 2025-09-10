#pragma once
#include <Tbx/Layers/Layer.h>
#include <Tbx/TBS/Toy.h>
#include <Tbx/Graphics/Material.h>

namespace Tbx { class World; }

class ExampleLayer : public Tbx::Layer
{
public:
    ExampleLayer(const std::string& name, std::shared_ptr<Tbx::World> world)
        : Layer(name), _world(std::move(world)) {}

    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate() override;

private:
    std::shared_ptr<Tbx::World> _world = nullptr;
    std::shared_ptr<Tbx::Toy> _fpsCam = nullptr;
    std::shared_ptr<Tbx::Toy> _smily = nullptr;

    float _smilyBobTime = 0.0f;
    float _smilyBobAmplitude = 0.0f;

    float _camPitch = 0.0f;
    float _camYaw = 0.0f;

    Tbx::Material _simpleTexturedMat;
};

