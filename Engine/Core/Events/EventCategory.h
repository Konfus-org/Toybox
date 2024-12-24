#pragma once
#include "Math/BitOperations.h"
#include "tbxAPI.h"

namespace Toybox
{
    enum TBX_API EventCategory
    {
        None = 0,
        Input = BIT(0),
        Mouse = BIT(1),
        Keyboard = BIT(2),
        Controller = BIT(3),
        Application = BIT(4),
    };
}