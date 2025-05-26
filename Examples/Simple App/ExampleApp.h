#pragma once
#include <Tbx/Runtime/App/App.h>
#include <Tbx/Core/Plugins/RegisterPlugin.h>

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
