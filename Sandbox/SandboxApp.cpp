#include "SandboxApp.h"
#include "TestLayer.h"

SandboxApp* SandboxApp::Instance;

SandboxApp::SandboxApp() : Tbx::App("Sandbox")
{
    Instance = this;
}

void SandboxApp::OnStart()
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
