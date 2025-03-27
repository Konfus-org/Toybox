#pragma once
#include <Toybox.h>

class SandboxApp : public Tbx::App
{
public:
    static SandboxApp* Instance;

    SandboxApp();

    void OnStart() override;
    void OnUpdate() override;
    void OnShutdown() override;
};
