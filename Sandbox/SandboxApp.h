#pragma once
#include <Toybox.h>

class SandboxApp : public Toybox::App
{
public:
    SandboxApp() : Toybox::App("Sandbox") {}

    void Launch() override;
};
