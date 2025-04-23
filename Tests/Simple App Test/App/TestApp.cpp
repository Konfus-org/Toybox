#include "TestApp.h"
#include "TestSceneLayer.h"

SandboxApp::SandboxApp() : Tbx::App("Sandbox")
{
}

void SandboxApp::OnLoad()
{
    // Do nothing
}

void SandboxApp::OnUnload()
{
    // Do nothing
}

void SandboxApp::OnLaunch()
{
    const auto& testLayer = std::make_shared<TestSceneLayer>("Testing");
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
