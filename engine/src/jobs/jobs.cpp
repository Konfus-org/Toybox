#include "tbx/jobs/jobs.h"
#include "tbx/debug/log.h"
#include <atomic>
#include <memory>

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

    //// JOBS ////

    Jobs::Jobs()
    {
        const uint cores = std::max(2u, std::thread::hardware_concurrency());
        const uint count = cores - 1;
        _workers.reserve(count);
        for (uint i = 0; i < count; ++i)
            _workers.emplace_back([this](std::stop_token stop) { worker_loop(stop); });
    }

    Jobs::~Jobs()
    {
        for (auto& worker : _workers)
            worker.request_stop();
        _worker_signal.notify_all();
        _workers.clear(); // joins
        drain_main();
    }

    void Jobs::drain_main()
    {
        std::vector<std::function<void()>> jobs;
        {
            std::scoped_lock lock(_main_mutex);
            jobs.swap(_main_queue);
        }
        // Jobs posted while draining run on the next drain — same one-frame rule as events.
        for (auto& job : jobs)
            job();
    }

    void Jobs::parallel_for(size count, const std::function<void(size)>& action)
    {
        if (count == 0)
            return;
        const size helpers = _workers.size();
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

    void Jobs::start(Task<void> task)
    {
        run_detached(std::move(task));
    }

    void Jobs::post_main(std::function<void()> job)
    {
        std::scoped_lock lock(_main_mutex);
        _main_queue.push_back(std::move(job));
    }

    void Jobs::post_worker(std::function<void()> job)
    {
        {
            std::scoped_lock lock(_worker_mutex);
            _worker_queue.push_back(std::move(job));
        }
        _worker_signal.notify_one();
    }

    void Jobs::worker_loop(std::stop_token stop)
    {
        while (true)
        {
            std::function<void()> job;
            {
                std::unique_lock lock(_worker_mutex);
                _worker_signal.wait(lock, stop, [this] { return !_worker_queue.empty(); });
                if (_worker_queue.empty())
                    return; // stop requested and nothing left to do
                job = std::move(_worker_queue.front());
                _worker_queue.pop_front();
            }
            job();
        }
    }
}
