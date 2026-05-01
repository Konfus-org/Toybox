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
        const auto* typed_message = dynamic_cast<const TMessage*>(&message);
        if (typed_message == nullptr)
            return std::nullopt;

        return std::cref(*typed_message);
    }

    template <typename TMessage>
    std::optional<std::reference_wrapper<TMessage>> handle_message(Message& message)
    {
        auto* typed_message = dynamic_cast<TMessage*>(&message);
        if (typed_message == nullptr)
            return std::nullopt;

        return std::ref(*typed_message);
    }
}
