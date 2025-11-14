#include "tbx/time/timer.h"
#include <utility>

namespace tbx
{
    namespace
    {
        std::chrono::steady_clock::time_point compute_deadline(
            const TimeSpan& span,
            std::chrono::steady_clock::time_point start_time)
        {
            return start_time + span.to_duration();
        }
    }

    Timer::Timer()
    {
        reset();
    }

    Timer Timer::for_ticks(std::size_t ticks)
    {
        Timer timer;
        timer._mode = Mode::Ticks;
        timer._remaining_ticks = ticks;
        timer._has_deadline = false;
        timer._time_up_signaled = false;
        return timer;
    }

    Timer Timer::for_time_span(const TimeSpan& span, std::chrono::steady_clock::time_point start_time)
    {
        Timer timer;
        timer._mode = Mode::Time;
        timer._has_deadline = !span.is_zero();
        timer._deadline = compute_deadline(span, start_time);
        timer._time_up_signaled = false;
        return timer;
    }

    void Timer::reset()
    {
        _cancellation_source = CancellationSource();
        _mode = Mode::None;
        _remaining_ticks = 0;
        _deadline = {};
        _has_deadline = false;
        _time_up_signaled = false;
    }

    void Timer::set_tick_callback(std::function<void(std::size_t remaining)> callback)
    {
        _tick_callback = std::move(callback);
    }

    void Timer::set_time_up_callback(std::function<void()> callback)
    {
        _time_up_callback = std::move(callback);
    }

    void Timer::set_cancel_callback(std::function<void()> callback)
    {
        _cancel_callback = std::move(callback);
    }

    bool Timer::tick()
    {
        if (is_cancelled())
        {
            return false;
        }

        if (_mode != Mode::Ticks)
        {
            return false;
        }

        if (_remaining_ticks == 0)
        {
            mark_time_up();
            return false;
        }

        --_remaining_ticks;
        if (_tick_callback)
        {
            _tick_callback(_remaining_ticks);
        }

        return true;
    }

    bool Timer::is_time_up(std::chrono::steady_clock::time_point now)
    {
        if (is_cancelled())
        {
            return false;
        }

        if (_mode == Mode::Ticks)
        {
            if (_remaining_ticks == 0)
            {
                mark_time_up();
                return true;
            }
            return false;
        }

        if (_mode == Mode::Time)
        {
            if (!_has_deadline)
            {
                mark_time_up();
                return true;
            }

            if (now >= _deadline)
            {
                mark_time_up();
                return true;
            }

            return false;
        }

        mark_time_up();
        return true;
    }

    void Timer::cancel()
    {
        if (_cancellation_source.is_cancelled())
        {
            return;
        }

        _cancellation_source.cancel();
        if (_cancel_callback)
        {
            _cancel_callback();
        }
    }

    CancellationToken Timer::get_token() const
    {
        return _cancellation_source.get_token();
    }

    void Timer::mark_time_up()
    {
        if (_time_up_signaled || is_cancelled())
        {
            return;
        }

        _time_up_signaled = true;
        if (_time_up_callback)
        {
            _time_up_callback();
        }
    }

    bool Timer::is_cancelled() const
    {
        return _cancellation_source.is_cancelled();
    }
}
