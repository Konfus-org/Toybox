#pragma once
#include "tbx/jobs/task.h"
#include "tbx/utils/api.h"
#include "tbx/utils/typedefs.h"
#include <condition_variable>
#include <deque>
#include <exception>
#include <functional>
#include <mutex>
#include <optional>
#include <semaphore>
#include <thread>
#include <vector>

// Thread pool plus coroutine scheduling — the engine's one parallelism mechanism. The state
// is runtime.jobs; the pool spins up on first use. Everything is callable from any thread
// except update(), which the main loop owns. The frame is phase-structured: structural
// sandbox mutation happens on the main thread only, jobs read/write disjoint data within a
// phase — the schedule points are the synchronization.
namespace tbx::jobs
{
    /// @brief
    /// Purpose: The jobs module's state, held by value on the Runtime: the pool and both
    /// queues. Workers spin up on first use and the destructor joins them.
    struct TBX_API JobsState
    {
        JobsState() = default;
        ~JobsState();

        JobsState(const JobsState&) = delete;
        JobsState& operator=(const JobsState&) = delete;

        std::mutex worker_mutex;
        std::condition_variable_any worker_signal;
        std::deque<std::function<void()>> worker_queue;
        std::mutex main_mutex;
        std::vector<std::function<void()>> main_queue;
        std::vector<std::jthread> workers;
    };

    /// @brief
    /// Purpose: Awaitable returned by jobs::on_worker() / jobs::on_main(); resumes the
    /// coroutine on the chosen thread of the pool it was made from.
    struct TBX_API ScheduleOn
    {
        bool await_ready() const noexcept
        {
            return false;
        }

        void await_suspend(std::coroutine_handle<> handle) const;
        void await_resume() const noexcept {}

        std::reference_wrapper<JobsState> jobs;
        bool resume_on_main = false;
    };

    /// @brief
    /// Purpose: Runs queued main-thread continuations. Called once per frame by the runtime's
    /// pump; work posted during a drain runs on the next drain.
    TBX_API void update(JobsState& jobs);

    /// @brief
    /// Purpose: Number of pool threads (excluding the main thread).
    TBX_API size get_worker_count(JobsState& jobs);

    /// @brief
    /// Purpose: Schedule point: `co_await jobs::on_main(runtime)` resumes on the main thread
    /// at the next update().
    TBX_API ScheduleOn on_main(JobsState& jobs);

    /// @brief
    /// Purpose: Schedule point: `co_await jobs::on_worker(runtime)` resumes on a pool thread.
    TBX_API ScheduleOn on_worker(JobsState& jobs);

    /// @brief
    /// Purpose: Runs fn(0..count-1) across the pool and blocks until every index ran. The
    /// calling thread participates, so nesting inside a worker is safe.
    TBX_API void parallel_for(JobsState& jobs, size count, const std::function<void(size)>& action);

    /// @brief
    /// Purpose: Posts a callable to the main-thread queue (runs at the next update()) —
    /// how background threads (watcher, streaming) marshal work back safely.
    TBX_API void post_main(JobsState& jobs, std::function<void()> job);

    /// @brief
    /// Purpose: Posts a callable straight onto the worker pool.
    TBX_API void post_worker(JobsState& jobs, std::function<void()> job);

    /// @brief
    /// Purpose: Runs a callable on a worker thread; await the returned task for its result.
    template <typename Fn>
    auto run(JobsState& jobs, Fn fn) -> Task<std::invoke_result_t<Fn>>;

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
