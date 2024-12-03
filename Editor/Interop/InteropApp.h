#pragma once
#include <Toybox.h>
#include "InteropApp.h"
#include "InteropAPI.h"

namespace Toybox::Interop
{
    class InteropApp : public App
    {
    public:
        InteropApp();
        ~InteropApp() = default;
    };
}