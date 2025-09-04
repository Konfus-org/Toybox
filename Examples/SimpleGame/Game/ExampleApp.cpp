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
    EmplaceLayer<ExampleLayer>("Testing");
}

void ExampleApp::OnUpdate()
{
    // Do nothing
}

void ExampleApp::OnShutdown()
{
    // Do nothing
}
