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

        TimeSpan to_time_span(
            std::chrono::steady_clock::duration duration,
            TimeUnit unit,
            bool clamp_to_zero = true)
        {
            if (clamp_to_zero && duration < std::chrono::steady_clock::duration::zero())
            {
                duration = std::chrono::steady_clock::duration::zero();
            }

            switch (unit)
            {
                case TimeUnit::Ticks:
                    return {static_cast<uint64>(duration.count()), TimeUnit::Ticks};
                case TimeUnit::Milliseconds:
                    return {
                        static_cast<uint64>(
                            std::chrono::duration_cast<std::chrono::milliseconds>(duration).count()),
                        TimeUnit::Milliseconds};
                case TimeUnit::Seconds:
                    return {
                        static_cast<uint64>(
                            std::chrono::duration_cast<std::chrono::seconds>(duration).count()),
                        TimeUnit::Seconds};
                case TimeUnit::Minutes:
                    return {
                        static_cast<uint64>(
                            std::chrono::duration_cast<std::chrono::minutes>(duration).count()),
                        TimeUnit::Minutes};
                case TimeUnit::Hours:
                    return {
                        static_cast<uint64>(
                            std::chrono::duration_cast<std::chrono::hours>(duration).count()),
                        TimeUnit::Hours};
                case TimeUnit::Days:
                default:
                    return {
                        static_cast<uint64>(
                            std::chrono::duration_cast<std::chrono::hours>(duration).count() / 24),
                        TimeUnit::Days};
            }
        }
    }

    Timer& Timer::operator=(const Timer& other)
    {
        if (this == &other)
        {
            return *this;
        }

        const auto current_tick_callback = tick_callback;
        const auto current_time_up_callback = time_up_callback;
        const auto current_cancel_callback = cancel_callback;

        time = other.time;
        time_left = other.time_left;
        cancellation_source = other.cancellation_source;
        _deadline = other._deadline;
        _has_deadline = other._has_deadline;
        _time_up_signaled = other._time_up_signaled;
        _cancel_signaled = other._cancel_signaled;

        tick_callback = other.tick_callback ? other.tick_callback : current_tick_callback;
        time_up_callback = other.time_up_callback ? other.time_up_callback : current_time_up_callback;
        cancel_callback = other.cancel_callback ? other.cancel_callback : current_cancel_callback;

        return *this;
    }

    Timer& Timer::operator=(Timer&& other)
    {
        if (this == &other)
        {
            return *this;
        }

        auto current_tick_callback = std::move(tick_callback);
        auto current_time_up_callback = std::move(time_up_callback);
        auto current_cancel_callback = std::move(cancel_callback);

        time = other.time;
        time_left = other.time_left;
        cancellation_source = std::move(other.cancellation_source);
        _deadline = other._deadline;
        _has_deadline = other._has_deadline;
        _time_up_signaled = other._time_up_signaled;
        _cancel_signaled = other._cancel_signaled;

        tick_callback = other.tick_callback ? std::move(other.tick_callback) : std::move(current_tick_callback);
        time_up_callback = other.time_up_callback
                               ? std::move(other.time_up_callback)
                               : std::move(current_time_up_callback);
        cancel_callback = other.cancel_callback
                              ? std::move(other.cancel_callback)
                              : std::move(current_cancel_callback);

        return *this;
    }

    Timer::Timer(
        const TimeSpan& time,
        std::chrono::steady_clock::time_point start_time,
        CancellationSource cancellation_source)
        : time(time)
        , time_left(time)
        , cancellation_source(std::move(cancellation_source))
    {
        _has_deadline = !time.is_zero();
        _deadline = compute_deadline(time, start_time);
        _time_up_signaled = false;
        _cancel_signaled = false;
    }

    void Timer::reset()
    {
        cancellation_source = CancellationSource();
        time = {};
        time_left = {};
        _deadline = {};
        _has_deadline = false;
        _time_up_signaled = false;
        _cancel_signaled = false;
    }

    bool Timer::is_time_up() const
    {
        return _time_up_signaled || time_left.value == 0;
    }

    bool Timer::tick(std::chrono::steady_clock::time_point now)
    {
        update_time_left(now, true);

        if (is_cancelled())
        {
            return false;
        }

        if (time_left.value == 0)
        {
            return false;
        }

        if (tick_callback)
        {
            auto remaining = time_left.to_duration();
            const auto remaining_ticks = remaining > std::chrono::steady_clock::duration::zero()
                                             ? static_cast<std::size_t>(remaining.count())
                                             : std::size_t(0);
            tick_callback(remaining_ticks);
        }

        return true;
    }

    void Timer::mark_time_up()
    {
        if (_time_up_signaled || is_cancelled())
        {
            return;
        }

        _time_up_signaled = true;

        if (time_up_callback)
        {
            time_up_callback();
        }
    }

    bool Timer::is_cancelled() const
    {
        return cancellation_source.is_cancelled();
    }

    void Timer::update_time_left(std::chrono::steady_clock::time_point now, bool signal_on_zero)
    {
        if (is_cancelled())
        {
            if (!_cancel_signaled && cancel_callback)
            {
                cancel_callback();
            }

            _cancel_signaled = true;
            time_left = {0, time.unit};
            return;
        }

        if (!_has_deadline)
        {
            time_left = {0, time.unit};

            if (signal_on_zero)
            {
                mark_time_up();
            }

            return;
        }

        auto remaining = _deadline - now;
        time_left = to_time_span(remaining, time.unit);

        if (time_left.value == 0 && signal_on_zero)
        {
            mark_time_up();
        }
    }
}
