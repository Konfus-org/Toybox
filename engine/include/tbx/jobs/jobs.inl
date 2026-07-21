#pragma once
// Template and inline bodies for Jobs/ScheduleOn — included by jobs.h.

namespace tbx
{
    template <typename Fn>
    auto Jobs::run(Fn fn) -> Task<std::invoke_result_t<Fn>>
    {
        co_await on_worker();
        if constexpr (std::is_void_v<std::invoke_result_t<Fn>>)
            fn();
        else
            co_return fn();
    }

    template <typename T>
    T Jobs::wait(Task<T> task)
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

    inline Task<void> Jobs::wrap_for_wait(
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
    Task<void> Jobs::wrap_for_wait(
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

    inline void ScheduleOn::await_suspend(std::coroutine_handle<> handle) const
    {
        if (resume_on_main)
            jobs.get().post_main([handle] { handle.resume(); });
        else
            jobs.get().post_worker([handle] { handle.resume(); });
    }
}
