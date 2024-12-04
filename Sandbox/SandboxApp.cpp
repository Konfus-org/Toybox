#include "SandboxApp.h"
#include "TestLayer.h"

SandboxApp::SandboxApp() : Toybox::App("Sandbox")
{
    PushLayer(new TestLayer("Testing"));
}
