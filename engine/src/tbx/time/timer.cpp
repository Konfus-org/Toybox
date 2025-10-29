#include "tbx/time/timer.h"

namespace tbx
{
    Timer::Timer()
    {
        reset();
    }

    void Timer::set_ticks(std::size_t ticks)
    {
        _use_ticks = ticks > 0;
        _remaining_ticks = ticks;
        _time_up_notified = false;
    }

    void Timer::set_time(const TimeSpan& delay, std::chrono::steady_clock::time_point now)
    {
        if (delay.is_zero())
        {
            _use_time = false;
            _ready_time = now;
            _time_up_notified = false;
            return;
        }

        set_time(delay.to_duration(), now);
    }

    void Timer::set_time(std::chrono::steady_clock::duration duration, std::chrono::steady_clock::time_point now)
    {
        if (duration <= std::chrono::steady_clock::duration::zero())
        {
            _use_time = false;
            _ready_time = now;
        }
        else
        {
            _use_time = true;
            _ready_time = now + duration;
        }

        _time_up_notified = false;
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
        if (token().is_cancelled())
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

    bool Timer::is_ready(std::chrono::steady_clock::time_point now) const
    {
        if (token().is_cancelled())
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

    CancellationToken Timer::token() const
    {
        return _cancellation.token();
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

