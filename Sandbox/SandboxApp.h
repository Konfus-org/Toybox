#pragma once
#include <Toybox.h>
#include <ToyboxEntryPoint.h>

class SandboxApp : public Toybox::Application::App
{
public:
    SandboxApp();
    ~SandboxApp() override = default;
};

Toybox::Application::App* Toybox::Application::CreateApp()
{
    return new SandboxApp();
}