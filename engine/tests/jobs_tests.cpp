#include "tbx/jobs/jobs.h"
#include <gtest/gtest.h>
#include <atomic>
#include <memory>
#include <thread>

namespace tbx::tests
{
    TEST(Jobs, RunReturnsCallableResultOnSuccess)
    {
        // Arrange
        jobs::reset();

        // Act
        const int result = jobs::wait(jobs::run([] { return 41 + 1; }));

        // Assert
        EXPECT_EQ(result, 42);
    }

    TEST(Jobs, WaitRethrowsWhenTaskThrows)
    {
        // Arrange
        jobs::reset();
        auto throwing = []() -> Task<void>
        {
            co_await jobs::on_worker();
            throw std::runtime_error("boom");
        };

        // Act / Assert
        EXPECT_THROW(jobs::wait(throwing()), std::runtime_error);
    }

    TEST(Jobs, RunExecutesOffTheCallingThread)
    {
        // Arrange
        jobs::reset();
        const auto main_thread = std::this_thread::get_id();

        // Act
        const auto worker_thread = jobs::wait(jobs::run([] { return std::this_thread::get_id(); }));

        // Assert
        EXPECT_NE(worker_thread, main_thread);
    }

    TEST(Jobs, TaskResultsChainAcrossAwaits)
    {
        // Arrange
        jobs::reset();
        auto inner = []() -> Task<int>
        {
            co_await jobs::on_worker();
            co_return 10;
        };
        auto outer = [&]() -> Task<int>
        {
            const int a = co_await inner();
            const int b = co_await inner();
            co_return a + b;
        };

        // Act
        const int result = jobs::wait(outer());

        // Assert
        EXPECT_EQ(result, 20);
    }

    TEST(Jobs, TaskCarriesMoveOnlyResults)
    {
        // Arrange
        jobs::reset();
        auto make = []() -> Task<std::unique_ptr<int>>
        {
            co_await jobs::on_worker();
            co_return std::make_unique<int>(7);
        };

        // Act
        auto result = jobs::wait(make());

        // Assert
        ASSERT_TRUE(result != nullptr);
        EXPECT_EQ(*result, 7);
    }

    TEST(Jobs, MainHopResumesOnDrain)
    {
        // Arrange
        jobs::reset();
        auto hopped = std::atomic<bool>(false);
        auto task = [&]() -> Task<void>
        {
            co_await jobs::on_worker();
            co_await jobs::on_main();
            hopped = true;
        };

        // Act: the coroutine parks itself on the main queue; nothing runs until we drain.
        jobs::start(task());
        for (int i = 0; i < 200 && !hopped; ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            jobs::drain_main();
        }

        // Assert
        EXPECT_TRUE(hopped);
    }

    TEST(Jobs, MainHopDoesNotRunWithoutDrain)
    {
        // Arrange
        jobs::reset();
        auto ran = std::atomic<bool>(false);
        auto task = [&]() -> Task<void>
        {
            co_await jobs::on_worker();
            co_await jobs::on_main();
            ran = true;
        };

        // Act: give the worker ample time to park the continuation, but never drain.
        jobs::start(task());
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Assert
        EXPECT_FALSE(ran);
        jobs::drain_main(); // let it finish before teardown
    }

    TEST(Jobs, ParallelForCoversEveryIndexExactlyOnce)
    {
        // Arrange
        jobs::reset();
        constexpr size COUNT = 10'000;
        auto hits = std::vector<std::atomic<int>>(COUNT);

        // Act
        jobs::parallel_for(COUNT, [&hits](size i) { hits[i].fetch_add(1); });

        // Assert
        for (size i = 0; i < COUNT; ++i)
            ASSERT_EQ(hits[i].load(), 1) << "index " << i;
    }

    TEST(Jobs, ParallelForWithZeroCountRunsNothing)
    {
        // Arrange
        jobs::reset();
        auto calls = std::atomic<int>(0);

        // Act
        jobs::parallel_for(0, [&calls](size) { calls.fetch_add(1); });

        // Assert
        EXPECT_EQ(calls.load(), 0);
    }

    TEST(Jobs, ParallelForNestsInsideWorker)
    {
        // Arrange
        jobs::reset();
        auto nested = []() -> Task<size>
        {
            co_await jobs::on_worker();
            auto sum = std::atomic<size>(0);
            jobs::parallel_for(100, [&sum](size i) { sum.fetch_add(i); });
            co_return sum.load();
        };

        // Act
        const size result = jobs::wait(nested());

        // Assert
        EXPECT_EQ(result, 4950u);
    }

    TEST(Jobs, DetachedTaskExceptionIsSwallowed)
    {
        // Arrange
        jobs::reset();
        auto throwing = []() -> Task<void>
        {
            co_await jobs::on_worker();
            throw std::runtime_error("detached boom");
        };

        // Act: logged, never propagated — surviving to the assert IS the behavior.
        jobs::start(throwing());
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Assert
        SUCCEED();
    }
}
