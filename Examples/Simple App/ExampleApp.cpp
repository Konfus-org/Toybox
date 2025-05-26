#include "ExampleApp.h"
#include "ExampleLayer.h"

ExampleApp::ExampleApp() : Tbx::App("Sandbox")
{
}

void ExampleApp::OnLoad()
{
    // Do nothing
}

void ExampleApp::OnUnload()
{
    // Do nothing
}

void ExampleApp::OnLaunch()
{
    const auto& testLayer = std::make_shared<ExampleLayer>("Testing");
    PushLayer(testLayer);
}

void ExampleApp::OnUpdate()
{
    // Do nothing
}

void ExampleApp::OnShutdown()
{
    // Do nothing
}
