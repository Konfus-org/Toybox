#include "tbx/utils/math_utils.h"

namespace tbx
{
    uint32 divide_round_up(const uint32 value, const uint32 divisor)
    {
        if (value == 0U)
            return 0U;

        return ((value - 1U) / divisor) + 1U;
    }
}
