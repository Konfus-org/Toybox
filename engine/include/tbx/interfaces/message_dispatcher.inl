#pragma once

namespace tbx
{
    template <typename TMessage>
        requires std::derived_from<TMessage, Message>
    Result IMessageDispatcher::send(TMessage& msg) const
    {
        return send(static_cast<Message&>(msg));
    }

    template <typename TMessage, typename... TArgs>
        requires(
            std::derived_from<TMessage, Message> && (sizeof...(TArgs) > 0)
            && std::is_constructible_v<TMessage, TArgs...>)
    Result IMessageDispatcher::send(TArgs&&... args) const
    {
        TMessage msg(std::forward<TArgs>(args)...);
        return send(msg);
    }

    template <typename TMessage>
        requires std::derived_from<TMessage, Message>
    std::shared_future<Result> IMessageDispatcher::post(const TMessage& msg) const
    {
        return post(std::make_unique<TMessage>(msg));
    }

    template <typename TMessage, typename... TArgs>
        requires(
            std::derived_from<TMessage, Message> && (sizeof...(TArgs) > 0)
            && std::is_constructible_v<TMessage, TArgs...>)
    std::shared_future<Result> IMessageDispatcher::post(TArgs&&... args) const
    {
        return post(std::make_unique<TMessage>(std::forward<TArgs>(args)...));
    }
}
