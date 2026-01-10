#include "pch.h"
#include "tbx/time/timer.h"

namespace tbx::tests::time
{
    TEST(timer_zero_span, signals_immediately)
    {
        bool time_up = false;

        Timer timer;
        timer.time_up_callback = [&]()
        {
            time_up = true;
        };

        timer = Timer(TimeSpan {});

        EXPECT_FALSE(timer.tick(TimeSpan {}));
        EXPECT_TRUE(time_up);
    }

    TEST(timer_timespan, waits_for_duration)
    {
        Timer timer(TimeSpan {});
        bool time_up = false;
        timer.time_up_callback = [&]()
        {
            time_up = true;
        };
        bool tick_called = false;
        timer.tick_callback = [&](uint64 remaining)
        {
            tick_called = true;
            (void)remaining;
        };

        TimeSpan span;
        span.value = 5;
        span.unit = TimeUnit::Milliseconds;

        timer = Timer(span);
        timer.time_up_callback = [&]()
        {
            time_up = true;
        };

        EXPECT_TRUE(timer.tick({1, TimeUnit::Milliseconds}));
        EXPECT_TRUE(tick_called);
        EXPECT_EQ(timer.time_left.value, 4);
        tick_called = false;

        EXPECT_FALSE(timer.tick({5, TimeUnit::Milliseconds}));
        EXPECT_FALSE(tick_called);
        EXPECT_TRUE(time_up);
    }

    TEST(timer_cancel, triggers_callback_and_blocks_ready)
    {
        Timer timer;
        bool cancelled = false;
        timer.cancel_callback = [&]()
        {
            cancelled = true;
        };

        EXPECT_FALSE(timer.cancellation_source.is_cancelled());
        timer.cancellation_source.cancel();
        EXPECT_FALSE(cancelled);
        EXPECT_FALSE(timer.tick(TimeSpan {}));
        EXPECT_TRUE(cancelled);
        EXPECT_TRUE(timer.cancellation_source.is_cancelled());
        EXPECT_FALSE(timer.tick(TimeSpan {}));
    }
}
