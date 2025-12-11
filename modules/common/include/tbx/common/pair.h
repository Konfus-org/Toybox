#pragma once
#include "tbx/tbx_api.h"
#include <utility>

namespace tbx
{
    template <typename TFirst, typename TSecond>
    struct Pair
    {
        Pair() = default;

        Pair(const TFirst& first_value, const TSecond& second_value)
            : first(first_value)
            , second(second_value)
        {
        }

        Pair(TFirst&& first_value, TSecond&& second_value)
            : first(std::move(first_value))
            , second(std::move(second_value))
        {
        }

        template <typename UFirst, typename USecond>
        Pair(UFirst&& first_value, USecond&& second_value)
            : first(std::forward<UFirst>(first_value))
            , second(std::forward<USecond>(second_value))
        {
        }

        TFirst first;
        TSecond second;
    };
}
