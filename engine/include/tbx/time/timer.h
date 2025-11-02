#pragma once
#include "tbx/state/cancellation_token.h"
#include "tbx/tbx_api.h"
#include "tbx/time/time_span.h"
#include <chrono>
#include <functional>
#include <utility>

namespace tbx
{
    // Thread-safe countdown utility that owns callbacks and cancellation state.
    class TBX_API Timer
    {
       public:
        using TickCallback = std::function<void(std::size_t)>;
        using Callback = std::function<void()>;

        Timer();
        Timer(
            std::size_t ticks,
            std::chrono::steady_clock::time_point ready_time,
            bool enable_ticks,
            bool enable_time);

        static Timer for_ticks(std::size_t ticks);
        static Timer for_time_span(
            const TimeSpan& delay,
            std::chrono::steady_clock::time_point now);
        static Timer for_duration(
            std::chrono::steady_clock::duration duration,
            std::chrono::steady_clock::time_point now);
        static Timer for_ticks_and_span(
            std::size_t ticks,
            const TimeSpan& delay,
            std::chrono::steady_clock::time_point now);
        static Timer for_ticks_and_duration(
            std::size_t ticks,
            std::chrono::steady_clock::duration duration,
            std::chrono::steady_clock::time_point now);

        void set_tick_callback(TickCallback callback)
        {
            _on_tick = std::move(callback);
        }
        void set_time_up_callback(Callback callback)
        {
            _on_time_up = std::move(callback);
        }
        void set_cancel_callback(Callback callback)
        {
            _on_cancel = std::move(callback);
        }

        void reset();

        bool tick();
        bool is_time_up(std::chrono::steady_clock::time_point now) const;

        void cancel();
        CancellationToken get_token() const;

       private:
        void configure_ticks(std::size_t ticks, bool enable_ticks);
        void configure_ready_time(
            std::chrono::steady_clock::time_point ready_time,
            bool enable_time);
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
