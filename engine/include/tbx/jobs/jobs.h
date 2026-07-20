#pragma once
#include "tbx/core/typedefs.h"
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
    struct ScheduleOn
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
    class Jobs final
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
        /// Purpose: Runs a callable on a worker thread; await the returned task for its result.
        template <typename Fn>
        auto run(Fn fn) -> Task<std::invoke_result_t<Fn>>
        {
            co_await on_worker();
            if constexpr (std::is_void_v<std::invoke_result_t<Fn>>)
                fn();
            else
                co_return fn();
        }

        /// @brief
        /// Purpose: Fire-and-forget: the coroutine owns itself and self-destroys when done.
        /// Exceptions escaping a detached task are logged, never propagated.
        void start(Task<void> task);

        /// @brief
        /// Purpose: Blocks the calling thread until the task completes and returns its result.
        /// For main()-loop boundaries and tests — inside coroutines, co_await instead.
        template <typename T>
        T wait(Task<T> task)
        {
            std::binary_semaphore done(0);
            if constexpr (std::is_void_v<T>)
            {
                std::exception_ptr error;
                start(wrap_for_wait(std::move(task), done, error));
                done.acquire();
                if (error)
                    std::rethrow_exception(error);
            }
            else
            {
                std::optional<T> result;
                std::exception_ptr error;
                start(wrap_for_wait(std::move(task), done, result, error));
                done.acquire();
                if (error)
                    std::rethrow_exception(error);
                return std::move(*result);
            }
        }

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
            std::exception_ptr& error)
        {
            try
            {
                co_await std::move(task);
            }
            catch (...)
            {
                error = std::current_exception();
            }
            done.release();
        }

        template <typename T>
        static Task<void> wrap_for_wait(
            Task<T> task,
            std::binary_semaphore& done,
            std::optional<T>& result,
            std::exception_ptr& error)
        {
            try
            {
                result = co_await std::move(task);
            }
            catch (...)
            {
                error = std::current_exception();
            }
            done.release();
        }

        void post_main(std::function<void()> job);
        void post_worker(std::function<void()> job);
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

    inline void ScheduleOn::await_suspend(std::coroutine_handle<> handle) const
    {
        if (resume_on_main)
            jobs.get().post_main(
                [handle]
                {
                    handle.resume();
                });
        else
            jobs.get().post_worker(
                [handle]
                {
                    handle.resume();
                });
    }
}
