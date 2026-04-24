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
    std::optional<std::reference_wrapper<const TMessage>> handle_message(const Message& message)
    {
        try
        {
            return std::cref(dynamic_cast<const TMessage&>(message));
        }
        catch (const std::bad_cast&)
        {
            return std::nullopt;
        }
    }

    template <typename TMessage>
    std::optional<std::reference_wrapper<TMessage>> handle_message(Message& message)
    {
        try
        {
            return std::ref(dynamic_cast<TMessage&>(message));
        }
        catch (const std::bad_cast&)
        {
            return std::nullopt;
        }
    }
}
