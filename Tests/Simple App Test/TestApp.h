#pragma once
#include <Tbx/App/App.h>
#include <Tbx/Runtime/Runtime.h>

class SandboxApp : public Tbx::App
{
public:
    SandboxApp();

    void OnLaunch() override;
    void OnUpdate() override;
    void OnShutdown() override;

    void OnLoad() override;
    void OnUnload() override;

    Tbx::App* Provide() override;
    void Destroy(Tbx::App* toDestroy) override;
};
