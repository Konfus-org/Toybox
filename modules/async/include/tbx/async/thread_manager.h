#pragma once
#include "tbx/tbx_api.h"
#include <concepts>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace tbx
{
    /// <summary>
    /// Purpose: Owns named long-running worker threads and per-thread task queues.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns all registered lane threads and queued tasks for the manager lifetime.
    /// Thread Safety: `try_create_lane`, `has_lane`, `post`, `post_with_future`, `stop_lane`,
    /// `stop_all`, and `get_lane_count` are thread-safe.
    /// </remarks>
    class TBX_API ThreadManager final
    {
      public:
        using Task = std::move_only_function<void()>;

        /// <summary>
        /// Purpose: Constructs an empty thread manager with no worker lanes.
        /// </summary>
        /// <remarks>
        /// Ownership: Instance owns lane resources created after construction.
        /// Thread Safety: Construction is not thread-safe for the same instance.
        /// </remarks>
        ThreadManager() = default;

        /// <summary>
        /// Purpose: Stops all lanes and releases manager-owned resources.
        /// </summary>
        /// <remarks>
        /// Ownership: Releases all owned lane threads and queued tasks.
        /// Thread Safety: Not thread-safe against concurrent destruction and member calls.
        /// </remarks>
        ~ThreadManager() noexcept;

        ThreadManager(const ThreadManager&) = delete;
        ThreadManager& operator=(const ThreadManager&) = delete;
        ThreadManager(ThreadManager&&) = delete;
        ThreadManager& operator=(ThreadManager&&) = delete;

        /// <summary>
        /// Purpose: Creates a named thread lane when it does not already exist.
        /// </summary>
        /// <remarks>
        /// Ownership: Manager owns the created lane and its worker thread.
        /// Thread Safety: Thread-safe.
        /// </remarks>
        bool try_create_lane(std::string_view lane_name);

        /// <summary>
        /// Purpose: Checks whether a named lane is currently registered.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns value only; no ownership transfer.
        /// Thread Safety: Thread-safe.
        /// </remarks>
        bool has_lane(std::string_view lane_name) const;

        /// <summary>
        /// Purpose: Enqueues a fire-and-forget task onto the named lane.
        /// </summary>
        /// <remarks>
        /// Ownership: Transfers task ownership to the target lane queue.
        /// Thread Safety: Thread-safe. Throws when lane is missing or stopped.
        /// </remarks>
        void post(std::string_view lane_name, Task&& task);

        /// <summary>
        /// Purpose: Enqueues a callable onto the named lane and returns a completion future.
        /// </summary>
        /// <remarks>
        /// Ownership: Callable ownership is transferred to the lane queue; returned future is
        /// owned by the caller.
        /// Thread Safety: Thread-safe. Throws when lane is missing or stopped.
        /// </remarks>
        template <typename TCallable, typename... TArgs>
            requires std::invocable<TCallable, TArgs...>
        auto post_with_future(std::string_view lane_name, TCallable&& callable, TArgs&&... args)
            -> std::future<std::invoke_result_t<TCallable, TArgs...>>
        {
            using TResult = std::invoke_result_t<TCallable, TArgs...>;

            auto task = std::packaged_task<TResult()>(
                std::bind_front(std::forward<TCallable>(callable), std::forward<TArgs>(args)...));
            auto task_future = task.get_future();

            post(
                lane_name,
                [task = std::move(task)]() mutable
                {
                    task();
                });

            return task_future;
        }

        /// <summary>
        /// Purpose: Stops one lane and removes it from the manager.
        /// </summary>
        /// <remarks>
        /// Ownership: Releases ownership of the removed lane and drains queued work.
        /// Thread Safety: Thread-safe. No-op when lane does not exist.
        /// </remarks>
        void stop_lane(std::string_view lane_name);

        /// <summary>
        /// Purpose: Stops and removes all lanes.
        /// </summary>
        /// <remarks>
        /// Ownership: Releases ownership of all lanes and drains queued work.
        /// Thread Safety: Thread-safe and idempotent.
        /// </remarks>
        void stop_all();

        /// <summary>
        /// Purpose: Returns the count of currently registered lanes.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns value only; no ownership transfer.
        /// Thread Safety: Thread-safe.
        /// </remarks>
        std::size_t get_lane_count() const;

      private:
        class ThreadLane final
        {
          public:
            explicit ThreadLane(std::string lane_name);
            ~ThreadLane() noexcept;

            ThreadLane(const ThreadLane&) = delete;
            ThreadLane& operator=(const ThreadLane&) = delete;
            ThreadLane(ThreadLane&&) = delete;
            ThreadLane& operator=(ThreadLane&&) = delete;

            void post(Task&& task);
            void stop();

          private:
            void run(std::stop_token stop_token);

          private:
            std::string _name = {};
            std::jthread _worker = {};
            std::mutex _queue_mutex = {};
            std::condition_variable _queued_task_signal = {};
            std::deque<Task> _queued_tasks = {};
            bool _accepting_tasks = true;
        };

      private:
        std::shared_ptr<ThreadLane> get_lane(std::string_view lane_name) const;

      private:
        mutable std::mutex _lanes_mutex = {};
        std::unordered_map<std::string, std::shared_ptr<ThreadLane>> _lanes = {};
    };
}
