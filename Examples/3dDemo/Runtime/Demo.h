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
    Demo(const Tbx::WeakRef<Tbx::App>& app);
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

class DemoLoader : public Tbx::Plugin {
public: DemoLoader(Tbx::WeakRef<Tbx::App> app) : Tbx::Plugin(app) {
    app.lock()->AddLayer<Demo>(app);
}
}; extern "C" __declspec(dllexport) DemoLoader* Load(Tbx::WeakRef<Tbx::App> app) {
    auto plugin = new DemoLoader(app); return plugin;
} extern "C" __declspec(dllexport) void Unload(DemoLoader* pluginToUnload) {
    delete pluginToUnload;
};
