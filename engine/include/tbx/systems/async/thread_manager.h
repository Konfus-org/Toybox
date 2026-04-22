#pragma once
#include "tbx/tbx_api.h"
#include "tbx/types/typedefs.h"
#include <concepts>
#include <condition_variable>
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
    /// @brief
    /// Purpose: Owns named long-running worker threads and per-thread task queues.
    /// @details
    /// Ownership: Owns all registered lane threads and queued tasks for the manager lifetime.
    /// Thread Safety: `try_create_lane`, `has_lane`, `post`, `post_with_future`, `stop_lane`,
    /// `stop_all`, and `get_lane_count` are thread-safe.
    class TBX_API ThreadManager final
    {
      public:
        using Task = std::move_only_function<void()>;
        ThreadManager() = default;
        ~ThreadManager() noexcept;

        ThreadManager(const ThreadManager&) = delete;
        ThreadManager& operator=(const ThreadManager&) = delete;
        ThreadManager(ThreadManager&&) = delete;
        ThreadManager& operator=(ThreadManager&&) = delete;

        /// @brief
        /// Purpose: Creates a named thread lane when it does not already exist.
        /// @details
        /// Ownership: Manager owns the created lane and its worker thread.
        /// Thread Safety: Thread-safe.
        bool try_create_lane(std::string_view lane_name);

        /// @brief
        /// Purpose: Checks whether a named lane is currently registered.
        /// @details
        /// Ownership: Returns value only; no ownership transfer.
        /// Thread Safety: Thread-safe.
        bool has_lane(std::string_view lane_name) const;

        /// @brief
        /// Purpose: Enqueues a fire-and-forget task onto the named lane.
        /// @details
        /// Ownership: Transfers task ownership to the target lane queue.
        /// Thread Safety: Thread-safe. Throws when lane is missing or stopped.
        void post(std::string_view lane_name, Task&& task);

        /// @brief
        /// Purpose: Enqueues a callable onto the named lane and returns a completion future.
        /// @details
        /// Ownership: Callable ownership is transferred to the lane queue; returned future is owned
        /// by the caller. Thread Safety: Thread-safe. Throws when lane is missing or stopped.
        template <typename TCallable, typename... TArgs>
            requires std::invocable<TCallable, TArgs...>
        auto post_with_future(std::string_view lane_name, TCallable&& callable, TArgs&&... args)
            -> std::future<std::invoke_result_t<TCallable, TArgs...>>;

        /// @brief
        /// Purpose: Stops one lane and removes it from the manager.
        /// @details
        /// Ownership: Releases ownership of the removed lane and drains queued work.
        /// Thread Safety: Thread-safe. No-op when lane does not exist.
        void stop_lane(std::string_view lane_name);

        /// @brief
        /// Purpose: Stops and removes all lanes.
        /// @details
        /// Ownership: Releases ownership of all lanes and drains queued work.
        /// Thread Safety: Thread-safe and idempotent.
        void stop_all();

        /// @brief
        /// Purpose: Returns the count of currently registered lanes.
        /// @details
        /// Ownership: Returns value only; no ownership transfer.
        /// Thread Safety: Thread-safe.
        size get_lane_count() const;

      private:
        class ThreadLane final
        {
          public:
            ThreadLane(std::string lane_name);
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

#include "tbx/systems/async/thread_manager.inl"
