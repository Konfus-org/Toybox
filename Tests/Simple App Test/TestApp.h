#pragma once
#include <Tbx/App/App.h>

class SandboxApp : public Tbx::App
{
public:
    static SandboxApp* Instance;

    SandboxApp();

    void OnLaunch() override;
    void OnUpdate() override;
    void OnShutdown() override;

    // Inherited via App
    void OnLoad() override;
    void OnUnload() override;
    Tbx::App* Provide() override;
    void Destroy(Tbx::App* toDestroy) override;
};
