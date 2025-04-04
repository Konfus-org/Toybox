#pragma once
#include <Tbx/App/App.h>

class SandboxApp : public Tbx::App
{
public:
    SandboxApp();

    void OnLaunch() override;
    void OnUpdate() override;
    void OnShutdown() override;

    void OnLoad() override;
    void OnUnload() override;
};
