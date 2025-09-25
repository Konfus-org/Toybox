#pragma once
#include <Tbx/App/App.h>
#include <Tbx/Stages/Toy.h>
#include <Tbx/Stages/Stage.h>
#include "Tbx/Memory/Refs.h"
#include <Tbx/Graphics/Material.h>
#include <Tbx/Layers/Layer.h>
#include <Tbx/Plugins/Plugin.h>

class Demo : public Tbx::Layer, public Tbx::Plugin<Demo>
{
public:
    Demo(const Tbx::WeakRef<Tbx::App>& app)
        : Layer("3d Demo")
        , _app(app)
    {
    }

    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate() override;

private:
    Tbx::Ref<Tbx::Stage> _world = nullptr;
    Tbx::Ref<Tbx::Toy> _fpsCam = nullptr;
    Tbx::Ref<Tbx::Toy> _smily = nullptr;

    float _smilyBobTime = 0.0f;
    float _smilyBobAmplitude = 0.0f;

    float _camPitch = 0.0f;
    float _camYaw = 0.0f;

    Tbx::Material _simpleTexturedMat = {};
    Tbx::WeakRef<Tbx::App> _app = {};
};

