#pragma once
#include "tbx/types/uuid.h"
#include <functional>
#include <limits>
#include <random>
#include <sstream>
#include <string>

namespace tbx::internal
{
    static uint32 combine_value(uint32 seed, uint32 value)
    {
        auto hashed = std::hash<uint32> {}(value);
        seed ^= hashed + 0x9e3779b9U + (seed << 6) + (seed >> 2);
        return seed;
    }

}
