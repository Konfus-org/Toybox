#include "SandboxApp.h"
#include "TestLayer.h"

SandboxApp::SandboxApp() : Toybox::Application::App("Sandbox")
{
    PushLayer(new TestLayer("Testing"));
}
