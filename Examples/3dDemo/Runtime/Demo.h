#pragma once
#include <Tbx/App/App.h>
#include <Tbx/App/Runtime.h>
#include <Tbx/Stages/Toy.h>
#include <Tbx/Stages/Stage.h>
#include "Tbx/Memory/Refs.h"
#include <Tbx/Graphics/Material.h>

class Demo : public Tbx::Runtime
{
public:
    Demo(const Tbx::WeakRef<Tbx::App>& app);

    void OnStart() final;
    void OnShutdown() final;
    void OnUpdate() final;

private:
    Tbx::Ref<Tbx::Stage> _world = nullptr;
    Tbx::Ref<Tbx::Toy> _fpsCam = nullptr;
    Tbx::Ref<Tbx::Toy> _smily = nullptr;

    float _smilyBobTime = 0.0f;
    float _smilyBobAmplitude = 0.0f;

    float _camPitch = 0.0f;
    float _camYaw = 0.0f;

    Tbx::Ref<Tbx::Material> _simpleTexturedMat = {};
    Tbx::WeakRef<Tbx::App> _app = {};
};

TBX_REGISTER_RUNTIME(Demo);
