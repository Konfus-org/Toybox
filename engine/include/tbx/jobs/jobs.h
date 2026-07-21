#pragma once
#include "tbx/utils/api.h"
#include "tbx/utils/typedefs.h"
#include "tbx/jobs/task.h"
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <semaphore>
#include <thread>
#include <vector>

namespace tbx
{
    class Jobs;

    /// @brief
    /// Purpose: Awaitable returned by Jobs::worker() / Jobs::main(); resumes the coroutine on
    /// the chosen thread.
    struct TBX_API ScheduleOn
    {
        bool await_ready() const noexcept
        {
            return false;
        }

        void await_suspend(std::coroutine_handle<> handle) const;

        void await_resume() const noexcept {}

        std::reference_wrapper<Jobs> jobs;
        bool resume_on_main = false;
    };

    /// @brief
    /// Purpose: Thread pool plus coroutine scheduling — the engine's one parallelism mechanism.
    /// @details
    /// Ownership: Owns its worker threads; joins them on destruction. Thread Safety: All public
    /// methods are callable from any thread except drain_main(), which the main loop owns. The
    /// frame is phase-structured: structural sandbox mutation happens on the main thread only,
    /// jobs read/write disjoint data within a phase — the schedule points are the synchronization.
    class TBX_API Jobs final
    {
      public:
        Jobs();
        ~Jobs();

      public:
        Jobs(const Jobs&) = delete;
        Jobs& operator=(const Jobs&) = delete;

      public:
        /// @brief
        /// Purpose: Runs queued main-thread continuations. Called once per frame by
        /// Engine::pump(); work posted during a drain runs on the next drain.
        void drain_main();

        /// @brief
        /// Purpose: Schedule point: `co_await jobs.on_main()` resumes on the main thread at the
        /// next drain_main().
        ScheduleOn on_main()
        {
            return {.jobs = *this, .resume_on_main = true};
        }

        /// @brief
        /// Purpose: Runs fn(0..count-1) across the pool and blocks until every index ran. The
        /// calling thread participates, so nesting inside a worker is safe.
        void parallel_for(size count, const std::function<void(size)>& action);

        /// @brief
        /// Purpose: Posts a callable to the main-thread queue (runs at the next drain_main()) —
        /// how background threads (watcher, streaming) marshal work back safely.
        void post_main(std::function<void()> job);

        /// @brief
        /// Purpose: Posts a callable straight onto the worker pool.
        void post_worker(std::function<void()> job);

        /// @brief
        /// Purpose: Runs a callable on a worker thread; await the returned task for its result.
        template <typename Fn>
        auto run(Fn fn) -> Task<std::invoke_result_t<Fn>>;

        /// @brief
        /// Purpose: Fire-and-forget: the coroutine owns itself and self-destroys when done.
        /// Exceptions escaping a detached task are logged, never propagated.
        void start(Task<void> task);

        /// @brief
        /// Purpose: Blocks the calling thread until the task completes and returns its result.
        /// For main()-loop boundaries and tests — inside coroutines, co_await instead.
        template <typename T>
        T wait(Task<T> task);

        /// @brief
        /// Purpose: Schedule point: `co_await jobs.on_worker()` resumes on a pool thread.
        ScheduleOn on_worker()
        {
            return {.jobs = *this, .resume_on_main = false};
        }

        /// @brief
        /// Purpose: Number of pool threads (excluding the main thread).
        size get_worker_count() const
        {
            return _workers.size();
        }

      private:
        static Task<void> wrap_for_wait(
            Task<void> task,
            std::binary_semaphore& done,
            std::exception_ptr& error);

        template <typename T>
        static Task<void> wrap_for_wait(
            Task<T> task,
            std::binary_semaphore& done,
            std::optional<T>& result,
            std::exception_ptr& error);

        void worker_loop(std::stop_token stop);

      private:
        std::mutex _worker_mutex;
        std::condition_variable_any _worker_signal;
        std::deque<std::function<void()>> _worker_queue;
        std::mutex _main_mutex;
        std::vector<std::function<void()>> _main_queue;
        std::vector<std::jthread> _workers;

        friend struct ScheduleOn;
    };
}

#include "tbx/jobs/jobs.inl"
