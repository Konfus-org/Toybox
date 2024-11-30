#include "SandboxApp.h"
#include "TestLayer.h"

SandboxApp::SandboxApp() : Toybox::Application::App("Sandbox")
{
#ifdef TBX_PLATFORM_WINDOWS
    PushLayer(new TestLayer("Testing"));
#endif
}
