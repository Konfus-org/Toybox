#include "SandboxApp.h"
#include "TestLayer.h"


SandboxApp::SandboxApp() : Toybox::App("Sandbox")
{
    const auto& testLayer = std::make_shared<TestLayer>("Testing");
    PushLayer(testLayer);
}
