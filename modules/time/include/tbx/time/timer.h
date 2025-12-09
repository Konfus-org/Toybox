#pragma once
#include "tbx/async/cancellation_token.h"
#include "tbx/tbx_api.h"
#include "tbx/time/time_span.h"
#include <chrono>
#include <cstddef>
#include <functional>

namespace tbx
{
    // Schedules work using steady-clock deadlines derived from TimeSpan.
    // Ownership: Callers hold Timer instances by value; registered callbacks are non-owning
    // references whose lifetimes must exceed the timer's. The returned cancellation token
    // references the timer's internal cancellation source and becomes invalid after reset.
    // Thread-safety: Not inherently thread-safe. Callers must synchronize access when invoking
    // mutating functions from multiple threads.
    class TBX_API Timer
    {
      public:
        Timer(
            const TimeSpan& time = {},
            std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now(),
            CancellationSource cancellation_source = CancellationSource());

        void reset();

        bool tick(std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());

        bool is_time_up() const;

        TimeSpan time = {};
        TimeSpan time_left = {};
        std::function<void(std::size_t)> tick_callback;
        std::function<void()> time_up_callback;
        std::function<void()> cancel_callback;
        CancellationSource cancellation_source;

      private:
        void mark_time_up();
        bool is_cancelled() const;
        void update_time_left(std::chrono::steady_clock::time_point now, bool signal_on_zero);

        std::chrono::steady_clock::time_point _deadline = {};
        bool _has_deadline = false;
        bool _time_up_signaled = false;
        bool _cancel_signaled = false;
    };
}
