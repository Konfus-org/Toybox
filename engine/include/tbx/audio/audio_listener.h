#pragma once
#include "tbx/utils/api.h"

namespace tbx
{
    /// @brief
    /// Purpose: The ears: sounds spatialize relative to the first enabled listener's Transform.
    struct TBX_API AudioListener
    {
        float volume = 1.0f;
    };
}
