#pragma once
#include "tbx/common/typedefs.h"
#include "tbx/tbx_api.h"
#include <concepts>
#include <condition_variable>
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
    /// @brief
    /// Purpose: Configures worker allocation for the job scheduler.
    /// @details
    /// Ownership: Value type owned by callers and copied into the job system constructor.
    /// Thread Safety: Safe for concurrent use because it stores only plain data.
    struct TBX_API JobSystemConfiguration
    {
        size worker_count = {};
    };

    /// @brief
    /// Purpose: Schedules and executes asynchronous jobs on a managed worker pool.
    /// @details
    /// Ownership: Owns worker threads and queued jobs for the lifetime of the instance.
    /// Thread Safety: `schedule`, `schedule_with_future`, `wait_for_idle`, `stop`, and
    /// `get_worker_count` are safe to call concurrently. Destruction must be externally
    /// synchronized against concurrent use.
    class TBX_API JobSystem
    {
      public:
        using Job = std::move_only_function<void()>;
        JobSystem(const JobSystemConfiguration& configuration = {});
        ~JobSystem() noexcept;

        JobSystem(const JobSystem&) = delete;
        JobSystem& operator=(const JobSystem&) = delete;
        JobSystem(JobSystem&&) = delete;
        JobSystem& operator=(JobSystem&&) = delete;

        /// @brief
        /// Purpose: Enqueues a fire-and-forget job for asynchronous execution.
        /// @details
        /// Ownership: Transfers ownership of the job callable into the scheduler queue.
        /// Thread Safety: Thread-safe. Throws if scheduling is attempted after `stop`.
        void schedule(Job&& job);

        /// @brief
        /// Purpose: Enqueues a callable and returns a future for its completion result.
        /// @details
        /// Ownership: Transfers the callable into the scheduler. Returned future is owned by the
        /// caller and stores result/exception state. Thread Safety: Thread-safe. Throws if
        /// scheduling is attempted after `stop`.
        template <typename TCallable, typename... TArgs>
            requires std::invocable<TCallable, TArgs...>
        auto schedule_with_future(TCallable&& callable, TArgs&&... args)
            -> std::future<std::invoke_result_t<TCallable, TArgs...>>;

        /// @brief
        /// Purpose: Blocks until no queued or currently running jobs remain.
        /// @details
        /// Ownership: Does not transfer ownership.
        /// Thread Safety: Thread-safe.
        void wait_for_idle();

        /// @brief
        /// Purpose: Stops accepting new jobs and joins all worker threads.
        /// @details
        /// Ownership: Retains queued job ownership until they are drained, then releases all worker
        /// ownership. Thread Safety: Thread-safe and idempotent.
        void stop();

        /// @brief
        /// Purpose: Returns the number of currently active worker threads.
        /// @details
        /// Ownership: Returns a value copy.
        /// Thread Safety: Thread-safe.
        size get_worker_count() const;

      private:
        void run_worker(std::stop_token stop_token);

      private:
        std::vector<std::jthread> _workers = {};
        mutable std::mutex _queue_mutex = {};
        std::condition_variable _queued_job_signal = {};
        std::condition_variable _idle_signal = {};
        std::deque<Job> _queued_jobs = {};
        size _active_jobs = {};
        bool _accepting_jobs = true;
    };
}

#include "tbx/async/job_system.inl"
