#pragma once
#include "tbx/tbx_api.h"
#include "tbx/types/typedefs.h"
#include <chrono>
#include <format>
#include <string>

namespace tbx
{
    enum TimeUnit
    {
        MILLISECONDS,
        SECONDS,
        MINUTES,
        HOURS,
        DAYS
    };

    // Value-type duration helper; thread-safe due to copy semantics.
    struct TBX_API TimeSpan
    {
        bool is_zero() const;
        std::chrono::steady_clock::duration to_duration() const;

        operator bool() const;
        operator int() const;
        operator std::chrono::steady_clock::duration() const;

        uint64 value = 0;
        TimeUnit unit = TimeUnit::MILLISECONDS;
    };

}

template <>
struct std::formatter<tbx::TimeSpan>
{
    constexpr auto parse(std::format_parse_context& ctx)
    {
        return _formatter.parse(ctx);
    }

    template <typename TFormatContext>
    auto format(const tbx::TimeSpan& time_span, TFormatContext& ctx) const
    {
        auto suffix = std::string_view("(unknown unit)");
        switch (time_span.unit)
        {
            case tbx::TimeUnit::MILLISECONDS:
                suffix = "ms";
                break;
            case tbx::TimeUnit::SECONDS:
                suffix = "s";
                break;
            case tbx::TimeUnit::MINUTES:
                suffix = "min";
                break;
            case tbx::TimeUnit::HOURS:
                suffix = "h";
                break;
            case tbx::TimeUnit::DAYS:
                suffix = "d";
                break;
            default:
                break;
        }

        return _formatter.format(std::format("{} {}", time_span.value, suffix), ctx);
    }

    std::formatter<std::string> _formatter;
};
