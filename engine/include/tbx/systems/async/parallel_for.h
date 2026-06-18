#pragma once
#include "tbx/systems/async/job_system.h"
#include "tbx/types/typedefs.h"
#include <algorithm>
#include <future>
#include <type_traits>
#include <utility>
#include <vector>

namespace tbx
{
    // Below this many items the worker hand-off (scheduling + futures) costs more than it saves, so
    // the range runs inline on the calling thread instead.
    inline constexpr size PARALLEL_FOR_MIN_ITEMS = 256U;

    /// @brief
    /// Purpose: Runs `fn(i)` for every index in [0, count), spreading the range across the job
    /// system's workers when it is large enough to be worth it, and running inline otherwise.
    /// @details
    /// Ownership: Borrows the job system; does not retain it. Blocks until every index has been
    /// processed, so `fn` and whatever it captures only need to outlive the call.
    /// Thread Safety: `fn` is invoked concurrently for distinct indices, so it must not touch shared
    /// state without synchronization. A null job system, a single worker, or a small range all run
    /// the whole range inline on the calling thread.
    template <typename TFn>
        requires std::invocable<TFn, size>
    void parallel_for(JobSystem* jobs, size count, TFn&& fn)
    {
        if (count == 0U)
            return;

        const size worker_count = jobs != nullptr ? jobs->get_worker_count() : 0U;
        if (worker_count <= 1U || count < PARALLEL_FOR_MIN_ITEMS)
        {
            for (size i = 0U; i < count; ++i)
                fn(i);
            return;
        }

        // One chunk per worker; the first chunk runs on this thread so the caller isn't left idle
        // while the workers run.
        const size chunk_count = std::min(worker_count, count);
        const size chunk_size = (count + chunk_count - 1U) / chunk_count;

        auto futures = std::vector<std::future<void>> {};
        futures.reserve(chunk_count - 1U);
        for (size chunk = 1U; chunk < chunk_count; ++chunk)
        {
            const size begin = chunk * chunk_size;
            if (begin >= count)
                break;

            const size end = std::min(begin + chunk_size, count);
            futures.push_back(jobs->schedule_with_future(
                [&fn, begin, end]()
                {
                    for (size i = begin; i < end; ++i)
                        fn(i);
                }));
        }

        // Process the first chunk here, then join the workers. Even if `fn` throws inline, the
        // scheduled chunks must finish before their futures (which reference `fn`) are destroyed.
        const size first_end = std::min(chunk_size, count);
        try
        {
            for (size i = 0U; i < first_end; ++i)
                fn(i);
        }
        catch (...)
        {
            for (auto& future : futures)
            {
                if (future.valid())
                    future.wait();
            }
            throw;
        }

        for (auto& future : futures)
            future.get();
    }
}
