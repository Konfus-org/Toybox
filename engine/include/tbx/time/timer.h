#pragma once
#include "tbx/core/cancellation_token.h"
#include "tbx/time/time_span.h"
#include <chrono>
#include <cstddef>
#include <functional>
#include <utility>

namespace tbx
{
    /// \brief Utility that tracks either tick-based or time-based countdowns and notifies callers
    /// when progress is made, the delay elapses, or cancellation occurs.
    class Timer
    {
    public:
        using TickCallback = std::function<void(std::size_t)>;
        using Callback = std::function<void()>;

        Timer();

        void set_ticks(std::size_t ticks);
        void set_time(const TimeSpan& delay, std::chrono::steady_clock::time_point now);
        void set_time(std::chrono::steady_clock::duration duration, std::chrono::steady_clock::time_point now);

        void set_tick_callback(TickCallback callback) { _on_tick = std::move(callback); }
        void set_time_up_callback(Callback callback) { _on_time_up = std::move(callback); }
        void set_cancel_callback(Callback callback) { _on_cancel = std::move(callback); }

        void reset();

        bool tick();
        [[nodiscard]] bool is_ready(std::chrono::steady_clock::time_point now) const;

        void cancel();
        [[nodiscard]] CancellationToken token() const;

    private:
        void fire_cancel() const;
        void fire_time_up() const;

        std::size_t _remaining_ticks = 0;
        bool _use_ticks = false;
        std::chrono::steady_clock::time_point _ready_time{};
        bool _use_time = false;

        TickCallback _on_tick;
        Callback _on_time_up;
        Callback _on_cancel;

        mutable bool _time_up_notified = false;
        mutable bool _cancel_notified = false;
        CancellationSource _cancellation;
    };
}
