#pragma once
#include "tbx/messages/cancellation_token.h"
#include "tbx/tbx_api.h"
#include "tbx/time/time_span.h"
#include <chrono>
#include <cstddef>
#include <functional>

namespace tbx
{
    // Schedules work using either tick counts or steady-clock deadlines.
    // Ownership: Callers hold Timer instances by value; registered callbacks are non-owning
    // references whose lifetimes must exceed the timer's. The returned cancellation token
    // references the timer's internal cancellation source and becomes invalid after reset.
    // Thread-safety: Not inherently thread-safe. Callers must synchronize access when invoking
    // mutating functions from multiple threads.
    class TBX_API Timer 
    {
      public:
        Timer();

        static Timer for_ticks(std::size_t ticks);
        static Timer for_time_span(
            const TimeSpan& span,
            std::chrono::steady_clock::time_point start_time);

        void reset();

        void set_tick_callback(std::function<void(std::size_t remaining)> callback);
        void set_time_up_callback(std::function<void()> callback);
        void set_cancel_callback(std::function<void()> callback);

        bool tick();
        bool is_time_up(std::chrono::steady_clock::time_point now);

        void cancel();
        CancellationToken get_token() const;

      private:
        enum class Mode
        {
            None,
            Ticks,
            Time
        };

        void mark_time_up();
        bool is_cancelled() const;

        CancellationSource _cancellation_source;
        Mode _mode = Mode::None;
        std::size_t _remaining_ticks = 0;
        std::chrono::steady_clock::time_point _deadline = {};
        bool _has_deadline = false;
        bool _time_up_signaled = false;

        std::function<void(std::size_t)> _tick_callback;
        std::function<void()> _time_up_callback;
        std::function<void()> _cancel_callback;
    };
}
