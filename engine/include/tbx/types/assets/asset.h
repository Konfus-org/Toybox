#pragma once
#include "tbx/systems/assets/serialization.h"
#include "tbx/tbx_api.h"
#include "tbx/types/typedefs.h"
#include "tbx/types/uuid.h"

namespace tbx
{
    /// @brief
    /// Purpose: Provides metadata common to serialized engine assets.
    /// @details
    /// Ownership: Value type inherited by asset payloads.
    /// Thread Safety: Safe to copy between threads; synchronize shared mutation externally.
    struct TBX_API Asset
    {
        Uuid id = {};
        uint32 version = 1U;
    };
}
