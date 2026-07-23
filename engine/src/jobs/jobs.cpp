#include "tbx/jobs/jobs.h"
#include "tbx/debug/log.h"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Fire-and-forget coroutine shell: starts immediately and self-destroys at the
    /// end, so the wrapped Task outlives the caller without anyone holding it.
    struct DetachedTask
    {
        /// @brief
        /// Purpose: Promise for DetachedTask; logs escaped exceptions instead of propagating.
        struct promise_type
        {
            DetachedTask get_return_object()
            {
                return {};
            }

            std::suspend_never initial_suspend() noexcept
            {
                return {};
            }

            std::suspend_never final_suspend() noexcept
            {
                return {};
            }

            void return_void() {}

            void unhandled_exception()
            {
                try
                {
                    std::rethrow_exception(std::current_exception());
                }
                catch (const std::exception& e)
                {
                    TBX_ERROR("unhandled exception in detached task: {}", e.what());
                }
                catch (...)
                {
                    TBX_ERROR("unhandled non-std exception in detached task");
                }
            }
        };
    };

    /// @brief
    /// Purpose: Work-stealing state for one parallel_for call. Heap-owned (shared_ptr) so
    /// straggler worker jobs that wake after the caller returned can still safely observe
    /// "nothing left to grab".
    struct ParallelForState
    {
        std::function<void(size)> action;
        size count = 0;
        size chunk = 1;
        std::atomic<size> next = 0;
        std::atomic<size> done = 0;
        std::binary_semaphore finished {0};
    };

    //// HELPERS ////

    static DetachedTask run_detached(Task<void> task)
    {
        co_await std::move(task);
    }

    static void run_parallel_chunks(const std::shared_ptr<ParallelForState>& state)
    {
        while (true)
        {
            const size start = state->next.fetch_add(state->chunk);
            if (start >= state->count)
                return;
            const size end = std::min(start + state->chunk, state->count);
            for (size i = start; i < end; ++i)
                state->action(i);
            if (state->done.fetch_add(end - start) + (end - start) == state->count)
                state->finished.release();
        }
    }

    static void worker_loop(JobsState& state, const std::stop_token& stop)
    {
        while (true)
        {
            std::function<void()> job;
            {
                std::unique_lock lock(state.worker_mutex);
                state.worker_signal.wait(
                    lock,
                    stop,
                    [&state]
                    {
                        return !state.worker_queue.empty();
                    });
                if (state.worker_queue.empty())
                    return; // stop requested and nothing left to do
                job = std::move(state.worker_queue.front());
                state.worker_queue.pop_front();
            }
            job();
        }
    }

    static JobsState& ensure_jobs_ready(JobsState& state)
    {
        if (state.workers.empty())
        {
            const uint cores = std::max(2u, std::thread::hardware_concurrency());
            const uint count = cores - 1;
            state.workers.reserve(count);
            for (uint i = 0; i < count; ++i)
                state.workers.emplace_back(
                    // Workers reference the state inside the heap-stable RuntimeState, never
                    // the Runtime handle — Runtime moves never touch it.
                    [&state](const std::stop_token stop)
                    {
                        worker_loop(state, stop);
                    });
        }
        return state;
    }

    static void post_main_to(JobsState& state, std::function<void()> job)
    {
        std::scoped_lock lock(state.main_mutex);
        state.main_queue.push_back(std::move(job));
    }

    static void post_worker_to(JobsState& state, std::function<void()> job)
    {
        {
            std::scoped_lock lock(state.worker_mutex);
            state.worker_queue.push_back(std::move(job));
        }
        state.worker_signal.notify_one();
    }

    //// BOUNDARY ////

    void internal::update_jobs(JobsState& state)
    {
        std::vector<std::function<void()>> jobs;
        {
            std::scoped_lock lock(state.main_mutex);
            jobs.swap(state.main_queue);
        }
        // Jobs posted while draining run on the next drain — same one-frame rule as events.
        for (auto& job : jobs)
            job();
    }

    size get_worker_count(JobsState& jobs)
    {
        return ensure_jobs_ready(jobs).workers.size();
    }

    ScheduleOn on_main(JobsState& jobs)
    {
        return {.jobs = jobs, .resume_on_main = true};
    }

    ScheduleOn on_worker(JobsState& jobs)
    {
        return {.jobs = jobs, .resume_on_main = false};
    }

    void parallel_for(JobsState& pool, size count, const std::function<void(size)>& action)
    {
        if (count == 0)
            return;
        JobsState& jobs = ensure_jobs_ready(pool);
        const size helpers = jobs.workers.size();
        if (count == 1 || helpers == 0)
        {
            for (size i = 0; i < count; ++i)
                action(i);
            return;
        }

        auto state = std::make_shared<ParallelForState>();
        state->action = action;
        state->count = count;
        state->chunk = std::max<size>(1, count / ((helpers + 1) * 4));

        for (size i = 0; i < helpers; ++i)
            post_worker_to(
                jobs,
                [state]
                {
                    run_parallel_chunks(state);
                });
        run_parallel_chunks(state); // the calling thread participates — safe from a worker too
        state->finished.acquire();
    }

    void post_main(JobsState& jobs, std::function<void()> job)
    {
        post_main_to(ensure_jobs_ready(jobs), std::move(job));
    }

    void post_worker(JobsState& jobs, std::function<void()> job)
    {
        post_worker_to(ensure_jobs_ready(jobs), std::move(job));
    }

    void start_detached(Task<void> task)
    {
        run_detached(std::move(task));
    }

    JobsState::~JobsState()
    {
        for (auto& worker : workers)
            worker.request_stop();
        worker_signal.notify_all();
        workers.clear(); // joins; queued work is dropped
    }

    void ScheduleOn::await_suspend(std::coroutine_handle<> handle) const
    {
        JobsState& pool = ensure_jobs_ready(jobs.get());
        if (resume_on_main)
            post_main_to(
                pool,
                [handle]
                {
                    handle.resume();
                });
        else
            post_worker_to(
                pool,
                [handle]
                {
                    handle.resume();
                });
    }
}
