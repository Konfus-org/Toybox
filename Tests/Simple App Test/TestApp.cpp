#include "TestApp.h"
#include "TestLayer.h"
#include <Tbx/Core/Plugins/RegisterPlugin.h>

SandboxApp::SandboxApp() : Tbx::App("Sandbox")
{
}

void SandboxApp::OnLoad()
{
    // Do nothing
}

void SandboxApp::OnLaunch()
{
    const auto& testLayer = std::make_shared<TestLayer>("Testing");
    PushLayer(testLayer);
}

void SandboxApp::OnUpdate()
{
    // Do nothing
}

void SandboxApp::OnShutdown()
{
    // Do nothing
}

void SandboxApp::OnUnload()
{
    // Do nothing
}

Tbx::App* SandboxApp::Provide()
{
    return new SandboxApp();
}

void SandboxApp::Destroy(Tbx::App* toDestroy)
{
    delete toDestroy;
}

TBX_REGISTER_PLUGIN(SandboxApp);
