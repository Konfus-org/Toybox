#include "tbx/time/timer.h"

namespace tbx
{
    std::chrono::steady_clock::duration Timer::Time::to_duration() const
    {
        std::chrono::steady_clock::duration duration{};

        duration += std::chrono::milliseconds(milliseconds);
        duration += std::chrono::seconds(seconds);
        duration += std::chrono::minutes(minutes);
        duration += std::chrono::hours(hours);

        if (days != 0)
        {
            duration += std::chrono::hours(days * 24);
        }

        return duration;
    }

    bool Timer::Time::is_zero() const
    {
        return milliseconds == 0 && seconds == 0 && minutes == 0 && hours == 0 && days == 0;
    }

    void Timer::set_ticks(std::size_t ticks)
    {
        _use_ticks = ticks > 0;
        _remaining_ticks = ticks;
    }

    void Timer::set_time(const Time& time, std::chrono::steady_clock::time_point now)
    {
        if (time.is_zero())
        {
            _use_time = false;
            _ready_time = now;
            return;
        }

        set_time(time.to_duration(), now);
    }

    void Timer::set_time(std::chrono::steady_clock::duration duration, std::chrono::steady_clock::time_point now)
    {
        if (duration <= std::chrono::steady_clock::duration::zero())
        {
            _use_time = false;
            _ready_time = now;
            return;
        }

        _use_time = true;
        _ready_time = now + duration;
    }

    void Timer::reset()
    {
        _use_ticks = false;
        _remaining_ticks = 0;
        _use_time = false;
        _ready_time = std::chrono::steady_clock::time_point{};
    }

    bool Timer::consume_tick()
    {
        if (!_use_ticks)
        {
            return false;
        }

        if (_remaining_ticks == 0)
        {
            return false;
        }

        --_remaining_ticks;
        return true;
    }

    bool Timer::has_delay() const
    {
        return _use_ticks || _use_time;
    }

    bool Timer::has_ticks() const
    {
        return _use_ticks;
    }

    bool Timer::has_time() const
    {
        return _use_time;
    }

    bool Timer::is_ready(std::chrono::steady_clock::time_point now) const
    {
        if (_use_time && now < _ready_time)
        {
            return false;
        }

        return true;
    }
}
