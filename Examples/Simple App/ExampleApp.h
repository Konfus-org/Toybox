#pragma once
#include <Tbx/Application/App/App.h>
#include <Tbx/Systems/Plugins/RegisterPlugin.h>

class ExampleApp : public Tbx::App
{
public:
    ExampleApp();

    void OnLaunch() override;
    void OnUpdate() override;
    void OnShutdown() override;

    void OnLoad() override;
    void OnUnload() override;
};

TBX_REGISTER_PLUGIN(ExampleApp);
