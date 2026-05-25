#pragma once
#include "tbx/tbx_api.h"
#include <chrono>
#include <format>
#include <string>

namespace tbx
{
    // Time delta between frames/updates.
    // Ownership: value type.
    // Thread-safety: value type; freely copyable.
    struct TBX_API DeltaTime
    {
        double seconds = 0.0;
        double milliseconds = 0.0;
    };

    // Simple per-thread timer to compute DeltaTime.
    // Thread-safety: Not thread-safe; use a separate instance per thread.
    class TBX_API DeltaTimer
    {
      public:
        DeltaTimer();

        // Resets internal state and starts timing from now
        void reset();

        // Advances the timer and returns the time since the previous tick
        DeltaTime tick();

      private:
        std::chrono::steady_clock::time_point _last;
    };

}

template <>
struct std::formatter<tbx::DeltaTime>
{
    constexpr auto parse(std::format_parse_context& ctx)
    {
        return _formatter.parse(ctx);
    }

    template <typename TFormatContext>
    auto format(const tbx::DeltaTime& delta_time, TFormatContext& ctx) const
    {
        return _formatter.format(std::format("{}s", delta_time.seconds), ctx);
    }

    std::formatter<std::string> _formatter;
};
