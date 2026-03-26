#include "pch.h"
#include "tbx/async/thread_manager.h"
#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>

namespace tbx::tests::async
{
    TEST(thread_manager, creates_and_queries_named_lane)
    {
        // Validates lane creation and lookup behavior.
        // Arrange
        auto thread_manager = ThreadManager {};

        // Act
        const bool did_create_lane = thread_manager.try_create_lane("render");

        // Assert
        EXPECT_TRUE(did_create_lane);
        EXPECT_TRUE(thread_manager.has_lane("render"));
        EXPECT_EQ(thread_manager.get_lane_count(), static_cast<std::size_t>(1));
    }

    TEST(thread_manager, rejects_duplicate_lane_names)
    {
        // Validates that duplicate lane registration is rejected.
        // Arrange
        auto thread_manager = ThreadManager {};
        EXPECT_TRUE(thread_manager.try_create_lane("render"));

        // Act
        const bool did_create_duplicate_lane = thread_manager.try_create_lane("render");

        // Assert
        EXPECT_FALSE(did_create_duplicate_lane);
        EXPECT_EQ(thread_manager.get_lane_count(), static_cast<std::size_t>(1));
    }

    TEST(thread_manager, post_executes_on_lane_thread)
    {
        // Validates fire-and-forget task execution on a dedicated lane.
        // Arrange
        auto thread_manager = ThreadManager {};
        ASSERT_TRUE(thread_manager.try_create_lane("render"));
        auto did_execute = std::atomic_bool(false);
        auto task_thread_id = std::thread::id {};
        auto completion = std::promise<void> {};

        // Act
        thread_manager.post(
            "render",
            [&did_execute, &task_thread_id, &completion]()
            {
                task_thread_id = std::this_thread::get_id();
                did_execute.store(true, std::memory_order_release);
                completion.set_value();
            });
        auto completion_future = completion.get_future();
        const auto wait_result = completion_future.wait_for(std::chrono::seconds(1));

        // Assert
        EXPECT_EQ(wait_result, std::future_status::ready);
        EXPECT_TRUE(did_execute.load(std::memory_order_acquire));
        EXPECT_NE(task_thread_id, std::this_thread::get_id());
    }

    TEST(thread_manager, post_with_future_returns_result)
    {
        // Validates request/reply task dispatch through futures.
        // Arrange
        auto thread_manager = ThreadManager {};
        ASSERT_TRUE(thread_manager.try_create_lane("physics"));

        // Act
        auto result_future = thread_manager.post_with_future(
            "physics",
            [](int left, int right)
            {
                return left + right;
            },
            20,
            22);

        // Assert
        EXPECT_EQ(result_future.get(), 42);
    }

    TEST(thread_manager, stop_lane_removes_lane_and_rejects_new_posts)
    {
        // Validates stop semantics for an individual lane.
        // Arrange
        auto thread_manager = ThreadManager {};
        ASSERT_TRUE(thread_manager.try_create_lane("render"));

        // Act
        thread_manager.stop_lane("render");

        // Assert
        EXPECT_FALSE(thread_manager.has_lane("render"));
        EXPECT_THROW(
            thread_manager.post(
                "render",
                []()
                {
                }),
            std::runtime_error);
    }

    TEST(thread_manager, stop_all_removes_all_lanes)
    {
        // Validates manager-wide stop and lane cleanup.
        // Arrange
        auto thread_manager = ThreadManager {};
        ASSERT_TRUE(thread_manager.try_create_lane("render"));
        ASSERT_TRUE(thread_manager.try_create_lane("physics"));

        // Act
        thread_manager.stop_all();

        // Assert
        EXPECT_EQ(thread_manager.get_lane_count(), static_cast<std::size_t>(0));
        EXPECT_FALSE(thread_manager.has_lane("render"));
        EXPECT_FALSE(thread_manager.has_lane("physics"));
    }
}
