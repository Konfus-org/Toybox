#pragma once
#include "tbx/app_description.h"

namespace tbx
{
    class Application;

    struct ApplicationContext
    {
        Application* instance = nullptr;
        AppDescription description = {};
    };
}