#include "tbx/jobs/jobs.h"
#include "tbx/debug/log.h"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>
#include <memory>

namespace tbx::jobs
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

            void return_void()
            {
            }

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

    //// STATE ////

    /// @brief
    /// Purpose: The whole module state: the pool and both queues, created on first use and
    /// destroyed by reset().
    struct JobsState
    {
        std::mutex worker_mutex;
        std::condition_variable_any worker_signal;
        std::deque<std::function<void()>> worker_queue;
        std::mutex main_mutex;
        std::vector<std::function<void()>> main_queue;
        std::vector<std::jthread> workers;

        ~JobsState()
        {
            for (auto& worker : workers)
                worker.request_stop();
            worker_signal.notify_all();
            workers.clear(); // joins
        }
    };

    static std::unique_ptr<JobsState> g_jobs = {};

    static void worker_loop(std::stop_token stop);

    static JobsState& ensure_jobs_ready()
    {
        if (!g_jobs)
        {
            g_jobs = std::make_unique<JobsState>();
            const uint cores = std::max(2u, std::thread::hardware_concurrency());
            const uint count = cores - 1;
            g_jobs->workers.reserve(count);
            for (uint i = 0; i < count; ++i)
                g_jobs->workers.emplace_back([](std::stop_token stop) { worker_loop(stop); });
        }
        return *g_jobs;
    }

    //// BOUNDARY ////

    void drain_main()
    {
        if (!g_jobs)
            return;
        std::vector<std::function<void()>> jobs;
        {
            std::scoped_lock lock(g_jobs->main_mutex);
            jobs.swap(g_jobs->main_queue);
        }
        // Jobs posted while draining run on the next drain — same one-frame rule as events.
        for (auto& job : jobs)
            job();
    }

    size get_worker_count()
    {
        return ensure_jobs_ready().workers.size();
    }

    void parallel_for(size count, const std::function<void(size)>& action)
    {
        if (count == 0)
            return;
        const size helpers = ensure_jobs_ready().workers.size();
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
            post_worker([state] { run_parallel_chunks(state); });
        run_parallel_chunks(state); // the calling thread participates — safe from a worker too
        state->finished.acquire();
    }

    void start(Task<void> task)
    {
        run_detached(std::move(task));
    }

    void reset()
    {
        g_jobs.reset(); // joins the pool; queued work is dropped
    }

    void post_main(std::function<void()> job)
    {
        JobsState& state = ensure_jobs_ready();
        std::scoped_lock lock(state.main_mutex);
        state.main_queue.push_back(std::move(job));
    }

    void post_worker(std::function<void()> job)
    {
        JobsState& state = ensure_jobs_ready();
        {
            std::scoped_lock lock(state.worker_mutex);
            state.worker_queue.push_back(std::move(job));
        }
        state.worker_signal.notify_one();
    }

    static void worker_loop(std::stop_token stop)
    {
        JobsState& state = *g_jobs;
        while (true)
        {
            std::function<void()> job;
            {
                std::unique_lock lock(state.worker_mutex);
                state.worker_signal.wait(
                    lock,
                    stop,
                    [&state] { return !state.worker_queue.empty(); });
                if (state.worker_queue.empty())
                    return; // stop requested and nothing left to do
                job = std::move(state.worker_queue.front());
                state.worker_queue.pop_front();
            }
            job();
        }
    }
}
