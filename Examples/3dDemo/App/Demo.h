#pragma once
#include <Tbx/App/App.h>
#include <Tbx/ECS/Toy.h>
#include <Tbx/ECS/ThreeDSpace.h>
#include <Tbx/Graphics/Material.h>
#include <Tbx/Layers/Layer.h>
#include <Tbx/Plugins/Plugin.h>

class Demo : public Tbx::Layer, public Tbx::Plugin
{
public:
    Demo(const std::weak_ptr<Tbx::App>& app)
        : Layer("3d Demo")
        , _app(app)
    {
    }

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

    Tbx::Material _simpleTexturedMat = {};
    std::weak_ptr<Tbx::App> _app = {};
};

TBX_REGISTER_PLUGIN(Demo);

