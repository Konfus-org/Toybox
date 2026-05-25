#pragma once
#include "tbx/systems/files/serialization.h"
#include "tbx/tbx_api.h"
#include "tbx/types/uuid.h"

namespace tbx
{
    /// @brief
    /// Purpose: Provides identity metadata common to public ECS components.
    /// @details
    /// Ownership: Value type inherited by component payloads. Copies preserve identity.
    /// Thread Safety: Safe to copy between threads; synchronize shared mutation externally.
    struct TBX_API Component
    {
        Uuid id = Uuid::generate();
    };
}
