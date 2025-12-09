#include "pch.h"
#include "tbx/time/timer.h"
#include <chrono>
#include <thread>

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

        auto now = std::chrono::steady_clock::now();
        timer = Timer(TimeSpan {}, now);

        EXPECT_FALSE(timer.tick(now));
        EXPECT_TRUE(time_up);
    }

    TEST(timer_timespan, waits_for_duration)
    {
        using namespace std::chrono_literals;

        Timer timer(TimeSpan {}, std::chrono::steady_clock::now());
        bool time_up = false;
        timer.time_up_callback = [&]()
        {
            time_up = true;
        };
        bool tick_called = false;
        timer.tick_callback = [&](std::size_t remaining)
        {
            tick_called = true;
            (void)remaining;
        };

        TimeSpan span;
        span.value = 5;
        span.unit = TimeUnit::Milliseconds;

        auto start = std::chrono::steady_clock::now();
        timer = Timer(span, start);
        timer.time_up_callback = [&]()
        {
            time_up = true;
        };

        EXPECT_TRUE(timer.tick(start));
        EXPECT_TRUE(tick_called);
        std::this_thread::sleep_for(6ms);
        tick_called = false;
        EXPECT_FALSE(timer.tick(std::chrono::steady_clock::now()));
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
        EXPECT_FALSE(timer.tick());
        EXPECT_TRUE(cancelled);
        EXPECT_TRUE(timer.cancellation_source.is_cancelled());
        EXPECT_FALSE(timer.tick());
    }
}
