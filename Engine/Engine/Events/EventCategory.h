#pragma once
#include "Math/BitOperations.h"

namespace Toybox::Events
{
    enum EventCategory
    {
        None = 0,
        Input = BIT(0),
        Mouse = BIT(1),
        Keyboard = BIT(2),
        Controller = BIT(3),
        Application = BIT(4),
    };
}