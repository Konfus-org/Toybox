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

    _windowCloseEventId = Tbx::Events::Subscribe<Tbx::WindowCloseEvent>(TBX_BIND_CALLBACK(TestingEventCallback));
}

void SandboxApp::OnUpdate()
{
    // Do nothing
}

void SandboxApp::OnShutdown()
{
    Tbx::Events::Unsubscribe<Tbx::WindowCloseEvent>(_windowCloseEventId);
}

void SandboxApp::TestingEventCallback(const Tbx::WindowCloseEvent& e)
{
    const auto& eventName = e.ToString();
    TBX_TRACE("Event received: {0}", eventName);
}
