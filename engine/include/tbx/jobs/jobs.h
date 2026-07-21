#pragma once
#include "tbx/utils/api.h"
#include "tbx/utils/typedefs.h"
#include "tbx/jobs/task.h"
#include <functional>
#include <optional>
#include <semaphore>

namespace tbx
{
    /// @brief
    /// Purpose: Awaitable returned by jobs::on_worker() / jobs::on_main(); resumes the
    /// coroutine on the chosen thread.
    struct TBX_API ScheduleOn
    {
        bool await_ready() const noexcept
        {
            return false;
        }

        void await_suspend(std::coroutine_handle<> handle) const;

        void await_resume() const noexcept {}

        bool resume_on_main = false;
    };
}

// Thread pool plus coroutine scheduling — the engine's one parallelism mechanism. The pool
// (module state) spins up on first use and purge() joins it. Everything is callable from any
// thread except drain_main(), which the main loop owns. The frame is phase-structured:
// structural sandbox mutation happens on the main thread only, jobs read/write disjoint data
// within a phase — the schedule points are the synchronization.
namespace tbx::jobs
{
    /// @brief
    /// Purpose: Runs queued main-thread continuations. Called once per frame by the runtime's
    /// pump; work posted during a drain runs on the next drain.
    TBX_API void drain_main();

    /// @brief
    /// Purpose: Number of pool threads (excluding the main thread).
    TBX_API size get_worker_count();

    /// @brief
    /// Purpose: Schedule point: `co_await jobs::on_main()` resumes on the main thread at the
    /// next drain_main().
    inline ScheduleOn on_main()
    {
        return {.resume_on_main = true};
    }

    /// @brief
    /// Purpose: Schedule point: `co_await jobs::on_worker()` resumes on a pool thread.
    inline ScheduleOn on_worker()
    {
        return {.resume_on_main = false};
    }

    /// @brief
    /// Purpose: Runs fn(0..count-1) across the pool and blocks until every index ran. The
    /// calling thread participates, so nesting inside a worker is safe.
    TBX_API void parallel_for(size count, const std::function<void(size)>& action);

    /// @brief
    /// Purpose: Posts a callable to the main-thread queue (runs at the next drain_main()) —
    /// how background threads (watcher, streaming) marshal work back safely.
    TBX_API void post_main(std::function<void()> job);

    /// @brief
    /// Purpose: Posts a callable straight onto the worker pool.
    TBX_API void post_worker(std::function<void()> job);

    /// @brief
    /// Purpose: Joins the pool and drops queued work; the next call starts fresh. run()
    /// calls this at shutdown.
    TBX_API void purge();

    /// @brief
    /// Purpose: Runs a callable on a worker thread; await the returned task for its result.
    template <typename Fn>
    auto run(Fn fn) -> Task<std::invoke_result_t<Fn>>;

    /// @brief
    /// Purpose: Fire-and-forget: the coroutine owns itself and self-destroys when done.
    /// Exceptions escaping a detached task are logged, never propagated.
    TBX_API void start(Task<void> task);

    /// @brief
    /// Purpose: Blocks the calling thread until the task completes and returns its result.
    /// For main()-loop boundaries and tests — inside coroutines, co_await instead.
    template <typename T>
    T wait(Task<T> task);
}

#include "tbx/jobs/jobs.inl"
