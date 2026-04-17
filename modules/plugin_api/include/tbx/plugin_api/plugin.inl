#pragma once

namespace tbx
{
    template <typename TMessage, typename... TArgs>
        requires std::derived_from<TMessage, Message>
    Result Plugin::send_message(TArgs&&... args) const
    {
        if (!_dispatcher)
        {
            TBX_ASSERT(_dispatcher, "Plugins must be attached before sending messages.");
            return dispatcher_missing_result("send a message");
        }

        if constexpr (sizeof...(TArgs) == 0)
        {
            static_assert(
                std::is_default_constructible_v<TMessage>,
                "Messages without constructor arguments must be default constructible.");
            TMessage msg = {};
            return _dispatcher->send(msg);
        }
        else
            return _dispatcher->send<TMessage>(std::forward<TArgs>(args)...);
    }

    template <typename TMessage, typename... TArgs>
        requires std::derived_from<TMessage, Message>
    std::shared_future<Result> Plugin::post_message(TArgs&&... args) const
    {
        if (!_dispatcher)
        {
            TBX_ASSERT(_dispatcher, "Plugins must be attached before posting messages.");
            std::promise<Result> promise;
            promise.set_value(dispatcher_missing_result("post a message"));
            return promise.get_future().share();
        }

        if constexpr (sizeof...(TArgs) == 0)
        {
            static_assert(
                std::is_default_constructible_v<TMessage>,
                "Messages without constructor arguments must be default constructible.");
            TMessage msg = {};
            return _dispatcher->post(msg);
        }
        else
            return _dispatcher->post<TMessage>(std::forward<TArgs>(args)...);
    }
}
