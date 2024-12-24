#include "SandboxApp.h"
#include "TestLayer.h"

void SandboxApp::Launch()
{
    const auto& testLayer = std::make_shared<TestLayer>("Testing");
    PushLayer(testLayer);
    Toybox::App::Launch();
}
