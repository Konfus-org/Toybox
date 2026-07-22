#pragma once
// Template and inline bodies for the jobs module — included by jobs.h.

namespace tbx::jobs
{
    template <typename Fn>
    auto run(State& jobs, Fn fn) -> Task<std::invoke_result_t<Fn>>
    {
        co_await on_worker(jobs);
        if constexpr (std::is_void_v<std::invoke_result_t<Fn>>)
            fn();
        else
            co_return fn();
    }

    /// @brief
    /// Purpose: wait()'s bridge: signals a semaphore when the task completes, capturing the
    /// result and any exception.
    inline Task<void> wrap_for_wait(
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
    Task<void> wrap_for_wait(
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
}
