#include "pch.h"
#include "tbx/time/timer.h"
#include <atomic>
#include <chrono>
#include <thread>

namespace tbx::tests::time
{
    TEST(timer_ticks, invokes_callbacks_until_ready)
    {
        Timer timer = Timer::for_ticks(2);
        std::size_t last_remaining = 0;
        std::atomic<int> tick_count{0};
        bool time_up = false;

        timer.set_tick_callback([&](std::size_t remaining)
        {
            last_remaining = remaining;
            tick_count.fetch_add(1);
        });
        timer.set_time_up_callback([&]()
        {
            time_up = true;
        });

        EXPECT_TRUE(timer.tick());
        EXPECT_EQ(last_remaining, 1u);
        EXPECT_EQ(tick_count.load(), 1);
        EXPECT_FALSE(time_up);
        EXPECT_FALSE(timer.is_time_up(std::chrono::steady_clock::now()));

        EXPECT_TRUE(timer.tick());
        EXPECT_EQ(last_remaining, 0u);
        EXPECT_EQ(tick_count.load(), 2);
        EXPECT_FALSE(time_up);

        EXPECT_FALSE(timer.tick());
        EXPECT_TRUE(timer.is_time_up(std::chrono::steady_clock::now()));
        EXPECT_TRUE(time_up);
    }

    TEST(timer_timespan, waits_for_duration)
    {
        using namespace std::chrono_literals;

        Timer timer = Timer::for_time_span(TimeSpan{}, std::chrono::steady_clock::now());
        bool time_up = false;
        timer.set_time_up_callback([&]()
        {
            time_up = true;
        });

        TimeSpan span;
        span.milliseconds = 5;

        auto start = std::chrono::steady_clock::now();
        timer = Timer::for_time_span(span, start);
        timer.set_time_up_callback([&]()
        {
            time_up = true;
        });

        EXPECT_FALSE(timer.is_time_up(start));
        std::this_thread::sleep_for(6ms);
        EXPECT_TRUE(timer.is_time_up(std::chrono::steady_clock::now()));
        EXPECT_TRUE(time_up);
    }

    TEST(timer_cancel, triggers_callback_and_blocks_ready)
    {
        Timer timer;
        bool cancelled = false;
        timer.set_cancel_callback([&]()
        {
            cancelled = true;
        });

        EXPECT_FALSE(timer.get_token().is_cancelled());
        timer.cancel();
        EXPECT_TRUE(cancelled);
        EXPECT_TRUE(timer.get_token().is_cancelled());
        EXPECT_FALSE(timer.tick());
        EXPECT_FALSE(timer.is_time_up(std::chrono::steady_clock::now()));
    }
}

