#pragma once
#include <Toybox.h>

static const SandboxApp& AppInstance;

class SandboxApp : public Tbx::App
{
public:
    SandboxApp();
};
