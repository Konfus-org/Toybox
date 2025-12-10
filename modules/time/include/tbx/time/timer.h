#pragma once
#include "tbx/async/cancellation_token.h"
#include "tbx/tbx_api.h"
#include "tbx/time/time_span.h"
#include <cstddef>
#include <functional>

namespace tbx
{
    // Schedules work using accumulated time deltas until the configured TimeSpan expires.
    // Ownership: Callers hold Timer instances by value; registered callbacks are non-owning
    // references whose lifetimes must exceed the timer's. The returned cancellation token
    // references the timer's internal cancellation source and becomes invalid after reset.
    // Thread-safety: Not inherently thread-safe. Callers must synchronize access when invoking
    // mutating functions from multiple threads.
    class TBX_API Timer
    {
      public:
        Timer(
            const TimeSpan& time_span = {},
            CancellationSource cancellation_source = CancellationSource());

        void reset();

        bool tick(const TimeSpan& delta_time);

        bool is_time_up() const;

        TimeSpan time_length = {};
        TimeSpan time_left = {};
        std::function<void(std::size_t)> tick_callback;
        std::function<void()> time_up_callback;
        std::function<void()> cancel_callback;
        CancellationSource cancellation_source;

      private:
        void mark_time_up();
        bool is_cancelled() const;
        void update_time_left(const TimeSpan& delta_time, bool signal_on_zero);

        bool _has_deadline = false;
        bool _time_up_signaled = false;
        bool _cancel_signaled = false;
    };
}
