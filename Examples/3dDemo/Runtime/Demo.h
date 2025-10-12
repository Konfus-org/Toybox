#pragma once
#include <Tbx/App/App.h>
#include <Tbx/App/Runtime.h>
#include <Tbx/Stages/Toy.h>
#include <Tbx/Stages/Stage.h>
#include "Tbx/Memory/Refs.h"
#include <Tbx/Graphics/Material.h>
#include <Tbx/Math/Vectors.h>

class Demo final : public Tbx::Runtime
{
public:
    Demo(Tbx::Ref<Tbx::EventBus> eventBus);
    ~Demo();

    void OnStart() override;
    void OnShutdown() override;
    void OnUpdate() override;

private:
    Tbx::Ref<Tbx::Stage> _stage = nullptr;
    Tbx::Ref<Tbx::Toy> _fpsCam = nullptr;
    Tbx::Ref<Tbx::Toy> _smily = nullptr;

    float _smilyBobTime = 0.0f;
    float _smilyBobAmplitude = 0.0f;
    float _smilyBaseHeight = 0.0f;

    float _camPitch = 0.0f;
    float _camYaw = 0.0f;
    Tbx::Vector2 _camLookVelocity = Tbx::Vector2::Zero;

    Tbx::Ref<Tbx::Material> _simpleTexturedMat = {};
    Tbx::WeakRef<Tbx::App> _app = {};
};

TBX_REGISTER_RUNTIME(Demo);
