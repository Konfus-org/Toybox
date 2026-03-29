#pragma once

namespace tbx
{
    template <typename TCallable, typename... TArgs>
        requires std::invocable<TCallable, TArgs...>
    auto JobSystem::schedule_with_future(TCallable&& callable, TArgs&&... args)
        -> std::future<std::invoke_result_t<TCallable, TArgs...>>
    {
        using TResult = std::invoke_result_t<TCallable, TArgs...>;

        auto task = std::packaged_task<TResult()>(
            std::bind_front(std::forward<TCallable>(callable), std::forward<TArgs>(args)...));
        auto task_future = task.get_future();

        schedule(
            [task = std::move(task)]() mutable
            {
                task();
            });

        return task_future;
    }
}
