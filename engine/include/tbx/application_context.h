#pragma once
#include "tbx/app_description.h"

namespace tbx
{
    class Application;

    struct TBX_API ApplicationContext
    {
        Application* instance = nullptr;
        AppDescription description = {};
    };
}
