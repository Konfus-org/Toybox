#include "SandboxApp.h"
#include "TestLayer.h"


SandboxApp::SandboxApp() : Tbx::App("Sandbox")
{
    const auto& testLayer = std::make_shared<TestLayer>("Testing");
    PushLayer(testLayer);
}
