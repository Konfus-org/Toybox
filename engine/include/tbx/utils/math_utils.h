#pragma once
#include "tbx/tbx_api.h"
#include "tbx/types/typedefs.h"

namespace tbx
{
    /// @brief
    /// Purpose: Divides a positive integer domain into fixed-size groups and rounds the result up.
    /// @details
    /// Ownership: Returns the computed group count by value.
    /// Thread Safety: Thread-safe; does not mutate shared state.
    TBX_API uint32 divide_round_up(uint32 value, uint32 divisor);
}
