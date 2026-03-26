#include "pch.h"
#include "tbx/async/job_system.h"
#include <atomic>
#include <chrono>
#include <stdexcept>

namespace tbx::tests::async
{
    TEST(job_system, uses_at_least_one_worker_by_default)
    {
        // Validates that default configuration provisions executable worker capacity.
        // Arrange
        JobSystem job_system;

        // Act
        auto worker_count = job_system.get_worker_count();

        // Assert
        EXPECT_GE(worker_count, static_cast<std::size_t>(1));
    }

    TEST(job_system, honors_explicit_worker_count)
    {
        // Validates that a caller-provided worker count is respected.
        // Arrange
        auto configuration = JobSystemConfiguration {.worker_count = 2};

        // Act
        JobSystem job_system(configuration);

        // Assert
        EXPECT_EQ(job_system.get_worker_count(), static_cast<std::size_t>(2));
    }

    TEST(job_system, executes_scheduled_jobs)
    {
        // Validates that queued jobs run and can be waited to completion.
        // Arrange
        JobSystem job_system(JobSystemConfiguration {.worker_count = 2});
        auto execution_count = std::atomic_int(0);

        // Act
        for (int index = 0; index < 16; ++index)
        {
            job_system.schedule(
                [&execution_count]()
                {
                    execution_count.fetch_add(1, std::memory_order_relaxed);
                });
        }
        job_system.wait_for_idle();

        // Assert
        EXPECT_EQ(execution_count.load(std::memory_order_relaxed), 16);
    }

    TEST(job_system, schedule_with_future_returns_results_and_exceptions)
    {
        // Validates return-value and exception propagation through futures.
        // Arrange
        JobSystem job_system;

        // Act
        auto result_future = job_system.schedule_with_future(
            [](int left, int right)
            {
                return left + right;
            },
            7,
            5);
        auto exception_future = job_system.schedule_with_future(
            []()
            {
                throw std::runtime_error("boom");
                return 0;
            });

        // Assert
        EXPECT_EQ(result_future.get(), 12);
        EXPECT_THROW(exception_future.get(), std::runtime_error);
    }

    TEST(job_system, stop_prevents_new_jobs_and_releases_workers)
    {
        // Validates stop behavior: no new scheduling after shutdown.
        // Arrange
        JobSystem job_system;
        auto started = std::atomic_bool(false);
        auto release = std::atomic_bool(false);

        job_system.schedule(
            [&started, &release]()
            {
                started.store(true, std::memory_order_release);

                while (!release.load(std::memory_order_acquire))
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
            });

        while (!started.load(std::memory_order_acquire))
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

        // Act
        release.store(true, std::memory_order_release);
        job_system.stop();

        // Assert
        EXPECT_EQ(job_system.get_worker_count(), static_cast<std::size_t>(0));
        EXPECT_THROW(
            job_system.schedule(
                []()
                {
                }),
            std::runtime_error);
    }
}
