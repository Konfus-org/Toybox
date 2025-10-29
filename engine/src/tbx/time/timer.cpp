#include "tbx/time/timer.h"

namespace tbx
{
    namespace
    {
        std::chrono::steady_clock::time_point compute_ready_time(const TimeSpan& span, std::chrono::steady_clock::time_point now, bool& enable_time)
        {
            if (span.is_zero())
            {
                enable_time = false;
                return now;
            }

            enable_time = true;
            return now + span.to_duration();
        }

        std::chrono::steady_clock::time_point compute_ready_time(std::chrono::steady_clock::duration duration, std::chrono::steady_clock::time_point now, bool& enable_time)
        {
            if (duration <= std::chrono::steady_clock::duration::zero())
            {
                enable_time = false;
                return now;
            }

            enable_time = true;
            return now + duration;
        }
    }

    Timer::Timer()
    {
        reset();
    }

    Timer::Timer(std::size_t ticks, std::chrono::steady_clock::time_point ready_time, bool enable_ticks, bool enable_time)
    {
        reset();
        configure_ticks(ticks, enable_ticks);
        configure_ready_time(ready_time, enable_time);
    }

    Timer Timer::for_ticks(std::size_t ticks)
    {
        return Timer(ticks, std::chrono::steady_clock::time_point{}, ticks > 0, false);
    }

    Timer Timer::for_time_span(const TimeSpan& delay, std::chrono::steady_clock::time_point now)
    {
        bool use_time = false;
        auto ready = compute_ready_time(delay, now, use_time);
        return Timer(0, ready, false, use_time);
    }

    Timer Timer::for_duration(std::chrono::steady_clock::duration duration, std::chrono::steady_clock::time_point now)
    {
        bool use_time = false;
        auto ready = compute_ready_time(duration, now, use_time);
        return Timer(0, ready, false, use_time);
    }

    Timer Timer::for_ticks_and_span(std::size_t ticks, const TimeSpan& delay, std::chrono::steady_clock::time_point now)
    {
        bool use_time = false;
        auto ready = compute_ready_time(delay, now, use_time);
        return Timer(ticks, ready, ticks > 0, use_time);
    }

    Timer Timer::for_ticks_and_duration(std::size_t ticks, std::chrono::steady_clock::duration duration, std::chrono::steady_clock::time_point now)
    {
        bool use_time = false;
        auto ready = compute_ready_time(duration, now, use_time);
        return Timer(ticks, ready, ticks > 0, use_time);
    }

    void Timer::reset()
    {
        _use_ticks = false;
        _remaining_ticks = 0;
        _use_time = false;
        _ready_time = std::chrono::steady_clock::time_point{};
        _time_up_notified = false;
        _cancel_notified = false;
        _cancellation = CancellationSource{};
    }

    bool Timer::tick()
    {
        if (get_token().is_cancelled())
        {
            fire_cancel();
            return false;
        }

        if (!_use_ticks || _remaining_ticks == 0)
        {
            return false;
        }

        --_remaining_ticks;

        if (_on_tick)
        {
            _on_tick(_remaining_ticks);
        }

        if (_remaining_ticks == 0)
        {
            _use_ticks = false;
        }

        return true;
    }

    bool Timer::is_time_up(std::chrono::steady_clock::time_point now) const
    {
        if (get_token().is_cancelled())
        {
            fire_cancel();
            return false;
        }

        if (_use_ticks)
        {
            return false;
        }

        if (_use_time && now < _ready_time)
        {
            return false;
        }

        fire_time_up();
        return true;
    }

    void Timer::cancel()
    {
        if (!_cancellation.is_cancelled())
        {
            _cancellation.cancel();
        }

        fire_cancel();
    }

    CancellationToken Timer::get_token() const
    {
        return _cancellation.token();
    }

    void Timer::configure_ticks(std::size_t ticks, bool enable_ticks)
    {
        _use_ticks = enable_ticks && ticks > 0;
        _remaining_ticks = _use_ticks ? ticks : 0;
        if (!_use_ticks)
        {
            _time_up_notified = false;
        }
    }

    void Timer::configure_ready_time(std::chrono::steady_clock::time_point ready_time, bool enable_time)
    {
        _use_time = enable_time;
        _ready_time = enable_time ? ready_time : std::chrono::steady_clock::time_point{};
        if (!enable_time)
        {
            _time_up_notified = false;
        }
    }

    void Timer::fire_cancel() const
    {
        if (_cancel_notified || !_cancellation.is_cancelled())
        {
            return;
        }

        _cancel_notified = true;
        if (_on_cancel)
        {
            _on_cancel();
        }
    }

    void Timer::fire_time_up() const
    {
        if (_time_up_notified)
        {
            return;
        }

        _time_up_notified = true;
        if (_on_time_up)
        {
            _on_time_up();
        }
    }
}

