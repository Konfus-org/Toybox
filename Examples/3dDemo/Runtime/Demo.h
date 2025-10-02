#pragma once
#include <Tbx/App/App.h>
#include <Tbx/App/Runtime.h>
#include <Tbx/Stages/Toy.h>
#include <Tbx/Stages/Stage.h>
#include "Tbx/Memory/Refs.h"
#include <Tbx/Graphics/Material.h>

class Demo final : public Tbx::Runtime
{
public:
    Demo(Tbx::Ref<Tbx::AssetServer> assetServer, Tbx::Ref<Tbx::EventBus> eventBus);
    ~Demo();

    void OnStart() override;
    void OnShutdown() override;
    void OnUpdate() override;

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
