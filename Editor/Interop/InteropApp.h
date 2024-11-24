#pragma once
#include <Toybox.h>
#include "InteropApp.h"
#include "InteropAPI.h"

namespace Toybox::Interop
{
    class InteropApp : public Application::App
    {
    public:
        InteropApp();
        ~InteropApp() = default;
    };
}