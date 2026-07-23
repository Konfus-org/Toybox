#pragma once
#include "tbx/api.h"

namespace tbx
{
    /// @brief
    /// Purpose: True once the builtin serializers are registered — the readiness check the
    /// engine subsystems assert before they read/write assets.
    TBX_DLL_EXPORT bool is_serialization_ready();

}
