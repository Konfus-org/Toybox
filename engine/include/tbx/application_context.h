#pragma once
#include "tbx/application.h"

namespace tbx
{
    struct ApplicationContext
    {
        Application* instance = nullptr;
        ApplicationDescription description = {};
    };
}