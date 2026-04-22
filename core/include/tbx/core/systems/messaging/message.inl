#pragma once

namespace tbx
{
    template <typename T>
    struct Request : public RequestBase
    {
        Request() = default;
        virtual ~Request() noexcept = default;

        T result = {};
    };

    template <>
    struct Request<void> : public RequestBase
    {
        Request() = default;
        virtual ~Request() noexcept = default;
    };

    template <typename TMessage>
    const TMessage* handle_message(const Message& message)
    {
        return dynamic_cast<const TMessage*>(&message);
    }

    template <typename TMessage>
    TMessage* handle_message(Message& message)
    {
        return dynamic_cast<TMessage*>(&message);
    }
}
