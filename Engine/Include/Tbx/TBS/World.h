#pragma once
#include "Tbx/DllExport.h"
#include <memory>

namespace Tbx
{
    class World
    {
    public:
        EXPORT void Update();
        EXPORT void Destroy();
    };
}