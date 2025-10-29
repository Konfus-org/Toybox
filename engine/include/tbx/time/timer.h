#pragma once
#include <chrono>
#include <cstddef>
#include <cstdint>

namespace tbx
{
    struct TimerDelay
    {
        std::int64_t milliseconds = 0;
        std::int64_t seconds = 0;
        std::int64_t minutes = 0;
        std::int64_t hours = 0;
        std::int64_t days = 0;

        std::chrono::steady_clock::duration to_duration() const;
        bool is_zero() const;
    };

    class Timer
    {
    public:
        Timer() = default;

        void set_ticks(std::size_t ticks);
        void set_time(const TimerDelay& delay, std::chrono::steady_clock::time_point now);
        void set_time(std::chrono::steady_clock::duration duration, std::chrono::steady_clock::time_point now);

        void reset();

        bool consume_tick();
        bool is_ready(std::chrono::steady_clock::time_point now) const;

    private:
        std::size_t _remaining_ticks = 0;
        bool _use_ticks = false;
        std::chrono::steady_clock::time_point _ready_time{};
        bool _use_time = false;
    };
}
