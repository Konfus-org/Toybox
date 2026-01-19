#include "tbx/time/timer.h"
#include <utility>

namespace tbx
{

    Timer::Timer(const TimeSpan& time_span, CancellationSource cancellation_source)
        : time_length(time_span)
        , time_left(time_span)
        , cancellation_source(std::move(cancellation_source))
    {
        _has_deadline = !time_length.is_zero();
        _time_up_signaled = false;
        _cancel_signaled = false;
    }

    Timer& Timer::operator=(const Timer& other)
    {
        if (this != &other)
        {
            time_length = other.time_length;
            time_left = other.time_left;
            cancellation_source = other.cancellation_source;
            _has_deadline = other._has_deadline;
            _time_up_signaled = other._time_up_signaled;
            _cancel_signaled = other._cancel_signaled;

            if (other.tick_callback)
                tick_callback = other.tick_callback;

            if (other.time_up_callback)
                time_up_callback = other.time_up_callback;

            if (other.cancel_callback)
                cancel_callback = other.cancel_callback;
        }

        return *this;
    }

    Timer& Timer::operator=(Timer&& other)
    {
        if (this != &other)
        {
            time_length = other.time_length;
            time_left = other.time_left;
            cancellation_source = std::move(other.cancellation_source);
            _has_deadline = other._has_deadline;
            _time_up_signaled = other._time_up_signaled;
            _cancel_signaled = other._cancel_signaled;

            if (other.tick_callback)
                tick_callback = std::move(other.tick_callback);

            if (other.time_up_callback)
                time_up_callback = std::move(other.time_up_callback);

            if (other.cancel_callback)
                cancel_callback = std::move(other.cancel_callback);
        }

        return *this;
    }

    void Timer::reset()
    {
        cancellation_source = CancellationSource();
        time_length = {};
        time_left = {};
        _has_deadline = false;
        _time_up_signaled = false;
        _cancel_signaled = false;
    }

    bool Timer::is_time_up() const
    {
        return _time_up_signaled || time_left.value == 0;
    }

    bool Timer::tick(const TimeSpan& delta_time)
    {
        update_time_left(delta_time, true);

        if (is_cancelled())
            return false;

        if (time_left.value == 0)
            return false;

        if (tick_callback)
            tick_callback(time_left.value);

        return true;
    }

    void Timer::mark_time_up()
    {
        if (_time_up_signaled || is_cancelled())
            return;

        _time_up_signaled = true;

        if (time_up_callback)
            time_up_callback();
    }

    bool Timer::is_cancelled() const
    {
        return cancellation_source.is_cancelled();
    }

    void Timer::update_time_left(const TimeSpan& delta_time, bool signal_on_zero)
    {
        if (is_cancelled())
        {
            if (!_cancel_signaled && cancel_callback)
                cancel_callback();

            _cancel_signaled = true;
            time_left = {0, time_length.unit};
            return;
        }

        if (!_has_deadline)
        {
            time_left = {0, time_length.unit};

            if (signal_on_zero)
                mark_time_up();

            return;
        }

        if (delta_time.value >= time_left.value)
        {
            time_left = {0, time_length.unit};

            if (signal_on_zero)
                mark_time_up();

            return;
        }

        time_left.value -= delta_time.value;
        time_left.unit = time_length.unit;
    }
}
