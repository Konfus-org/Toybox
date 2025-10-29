#pragma once

#include <chrono>
#include <cstdint>

namespace tbx
{
    /// \brief Describes a multi-unit duration that can be converted to a steady_clock duration.
    struct TimeSpan
    {
        std::int64_t milliseconds = 0;
        std::int64_t seconds = 0;
        std::int64_t minutes = 0;
        std::int64_t hours = 0;
        std::int64_t days = 0;

        [[nodiscard]] std::chrono::steady_clock::duration to_duration() const;
        [[nodiscard]] bool is_zero() const;
    };
}

