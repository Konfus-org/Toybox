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
        auto pool = jobs::JobsState();

        // Act
        const int result = jobs::wait(jobs::run(pool, [] { return 41 + 1; }));

        // Assert
        EXPECT_EQ(result, 42);
    }

    TEST(Jobs, WaitRethrowsWhenTaskThrows)
    {
        // Arrange
        auto pool = jobs::JobsState();
        auto throwing = [&]() -> jobs::Task<void>
        {
            co_await jobs::on_worker(pool);
            throw std::runtime_error("boom");
        };

        // Act / Assert
        EXPECT_THROW(jobs::wait(throwing()), std::runtime_error);
    }

    TEST(Jobs, RunExecutesOffTheCallingThread)
    {
        // Arrange
        auto pool = jobs::JobsState();
        const auto main_thread = std::this_thread::get_id();

        // Act
        const auto worker_thread = jobs::wait(jobs::run(pool, [] { return std::this_thread::get_id(); }));

        // Assert
        EXPECT_NE(worker_thread, main_thread);
    }

    TEST(Jobs, TaskResultsChainAcrossAwaits)
    {
        // Arrange
        auto pool = jobs::JobsState();
        auto inner = [&]() -> jobs::Task<int>
        {
            co_await jobs::on_worker(pool);
            co_return 10;
        };
        auto outer = [&]() -> jobs::Task<int>
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
        auto pool = jobs::JobsState();
        auto make = [&]() -> jobs::Task<std::unique_ptr<int>>
        {
            co_await jobs::on_worker(pool);
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
        auto pool = jobs::JobsState();
        auto hopped = std::atomic<bool>(false);
        auto task = [&]() -> jobs::Task<void>
        {
            co_await jobs::on_worker(pool);
            co_await jobs::on_main(pool);
            hopped = true;
        };

        // Act: the coroutine parks itself on the main queue; nothing runs until we drain.
        jobs::start(task());
        for (int i = 0; i < 200 && !hopped; ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            jobs::update(pool);
        }

        // Assert
        EXPECT_TRUE(hopped);
    }

    TEST(Jobs, MainHopDoesNotRunWithoutDrain)
    {
        // Arrange
        auto pool = jobs::JobsState();
        auto ran = std::atomic<bool>(false);
        auto task = [&]() -> jobs::Task<void>
        {
            co_await jobs::on_worker(pool);
            co_await jobs::on_main(pool);
            ran = true;
        };

        // Act: give the worker ample time to park the continuation, but never drain.
        jobs::start(task());
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Assert
        EXPECT_FALSE(ran);
        jobs::update(pool); // let it finish before teardown
    }

    TEST(Jobs, ParallelForCoversEveryIndexExactlyOnce)
    {
        // Arrange
        auto pool = jobs::JobsState();
        constexpr size COUNT = 10'000;
        auto hits = std::vector<std::atomic<int>>(COUNT);

        // Act
        jobs::parallel_for(pool, COUNT, [&hits](size i) { hits[i].fetch_add(1); });

        // Assert
        for (size i = 0; i < COUNT; ++i)
            ASSERT_EQ(hits[i].load(), 1) << "index " << i;
    }

    TEST(Jobs, ParallelForWithZeroCountRunsNothing)
    {
        // Arrange
        auto pool = jobs::JobsState();
        auto calls = std::atomic<int>(0);

        // Act
        jobs::parallel_for(pool, 0, [&calls](size) { calls.fetch_add(1); });

        // Assert
        EXPECT_EQ(calls.load(), 0);
    }

    TEST(Jobs, ParallelForNestsInsideWorker)
    {
        // Arrange
        auto pool = jobs::JobsState();
        auto nested = [&]() -> jobs::Task<size>
        {
            co_await jobs::on_worker(pool);
            auto sum = std::atomic<size>(0);
            jobs::parallel_for(pool, 100, [&sum](size i) { sum.fetch_add(i); });
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
        auto pool = jobs::JobsState();
        auto throwing = [&]() -> jobs::Task<void>
        {
            co_await jobs::on_worker(pool);
            throw std::runtime_error("detached boom");
        };

        // Act: logged, never propagated — surviving to the assert IS the behavior.
        jobs::start(throwing());
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Assert
        SUCCEED();
    }
}
