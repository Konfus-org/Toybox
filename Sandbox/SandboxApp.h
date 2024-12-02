#pragma once
#include <Toybox.h>
#include <ToyboxEntryPoint.h>

class SandboxApp : public Toybox::App
{
public:
    SandboxApp();
    ~SandboxApp() override = default;
};

Toybox::App* Toybox::CreateApp()
{
    return new SandboxApp();
}
