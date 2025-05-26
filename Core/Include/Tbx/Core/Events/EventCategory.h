#pragma once
#include "Tbx/Core/DllExport.h"
#include <Tbx/Math/Bits.h>

namespace Tbx
{
    enum class EXPORT EventCategory
    {
        None = 0,
        Input = BIT(0),
        Mouse = BIT(1),
        Keyboard = BIT(2),
        Controller = BIT(3),
        Application = BIT(4),
        Window = BIT(5),
        Render = BIT(6),
        World = BIT(6),
        Debug = BIT(7),
    };
}