#pragma once
#include "tbx/tbx_api.h"
#include <concepts>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace tbx
{
    /// <summary>
    /// Purpose: Configures worker allocation for the job scheduler.
    /// </summary>
    /// <remarks>
    /// Ownership: Value type owned by callers and copied into the job system constructor.
    /// Thread Safety: Safe for concurrent use because it stores only plain data.
    /// </remarks>
    struct TBX_API JobSystemConfiguration
    {
        std::size_t worker_count = {};
    };

    /// <summary>
    /// Purpose: Schedules and executes asynchronous jobs on a managed worker pool.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns worker threads and queued jobs for the lifetime of the instance.
    /// Thread Safety: `schedule`, `schedule_with_future`, `wait_for_idle`, `stop`, and
    /// `get_worker_count` are safe to call concurrently. Destruction must be externally
    /// synchronized against concurrent use.
    /// </remarks>
    class TBX_API JobSystem
    {
      public:
        using Job = std::move_only_function<void()>;

        /// <summary>
        /// Purpose: Creates a new job system and starts worker threads.
        /// </summary>
        /// <remarks>
        /// Ownership: The instance owns all worker threads and queued jobs.
        /// Thread Safety: Construction is not thread-safe for the same instance.
        /// </remarks>
        JobSystem(const JobSystemConfiguration& configuration = {});

        /// <summary>
        /// Purpose: Stops workers and drains queued jobs before releasing resources.
        /// </summary>
        /// <remarks>
        /// Ownership: Releases ownership of workers and queued jobs on destruction.
        /// Thread Safety: Not thread-safe against concurrent destruction and method calls.
        /// </remarks>
        ~JobSystem() noexcept;

        JobSystem(const JobSystem&) = delete;
        JobSystem& operator=(const JobSystem&) = delete;
        JobSystem(JobSystem&&) = delete;
        JobSystem& operator=(JobSystem&&) = delete;

        /// <summary>
        /// Purpose: Enqueues a fire-and-forget job for asynchronous execution.
        /// </summary>
        /// <remarks>
        /// Ownership: Transfers ownership of the job callable into the scheduler queue.
        /// Thread Safety: Thread-safe. Throws if scheduling is attempted after `stop`.
        /// </remarks>
        void schedule(Job&& job);

        /// <summary>
        /// Purpose: Enqueues a callable and returns a future for its completion result.
        /// </summary>
        /// <remarks>
        /// Ownership: Transfers the callable into the scheduler. Returned future is owned by
        /// the caller and stores result/exception state.
        /// Thread Safety: Thread-safe. Throws if scheduling is attempted after `stop`.
        /// </remarks>
        template <typename TCallable, typename... TArgs>
            requires std::invocable<TCallable, TArgs...>
        auto schedule_with_future(TCallable&& callable, TArgs&&... args)
            -> std::future<std::invoke_result_t<TCallable, TArgs...>>
        {
            using TResult = std::invoke_result_t<TCallable, TArgs...>;

            auto task = std::packaged_task<TResult()>(
                std::bind_front(std::forward<TCallable>(callable), std::forward<TArgs>(args)...));
            auto task_future = task.get_future();

            schedule(
                [task = std::move(task)]() mutable
                {
                    task();
                });

            return task_future;
        }

        /// <summary>
        /// Purpose: Blocks until no queued or currently running jobs remain.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not transfer ownership.
        /// Thread Safety: Thread-safe.
        /// </remarks>
        void wait_for_idle();

        /// <summary>
        /// Purpose: Stops accepting new jobs and joins all worker threads.
        /// </summary>
        /// <remarks>
        /// Ownership: Retains queued job ownership until they are drained, then releases all
        /// worker ownership.
        /// Thread Safety: Thread-safe and idempotent.
        /// </remarks>
        void stop();

        /// <summary>
        /// Purpose: Returns the number of currently active worker threads.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns a value copy.
        /// Thread Safety: Thread-safe.
        /// </remarks>
        std::size_t get_worker_count() const;

      private:
        void run_worker(std::stop_token stop_token);

      private:
        std::vector<std::jthread> _workers = {};
        mutable std::mutex _queue_mutex = {};
        std::condition_variable _queued_job_signal = {};
        std::condition_variable _idle_signal = {};
        std::deque<Job> _queued_jobs = {};
        std::size_t _active_jobs = {};
        bool _accepting_jobs = true;
    };
}
