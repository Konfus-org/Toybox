#pragma once
#include "tbx/async/cancellation_token.h"
#include "tbx/common/function_traits.h"
#include "tbx/common/uuid.h"
#include "tbx/messages/result.h"
#include "tbx/time/time_span.h"
#include <any>
#include <functional>

namespace tbx
{
    enum class MessageState
    {
        InProgress,
        Handled,
        Processed,
        Cancelled,
        TimedOut,
        Failed
    };

    struct Message;

    using MessageCallback = std::function<void(const Message&)>;

    struct TBX_API MessageCallbacks
    {
        MessageCallback on_failure;
        MessageCallback on_cancelled;
        MessageCallback on_handled;
        MessageCallback on_processed;
        MessageCallback on_timeout;
    };

    // Base polymorphic message type for dispatching.
    // Ownership: Messages are typically stack-allocated and passed by reference
    // to send(). When posted, a copy is stored by the coordinator until delivery.
    // The callbacks stored herein are non-owning; ensure captured state outlives
    // dispatch or use weak references.
    // Thread-safety: MessageCoordinator serializes access to Message instances,
    // allowing handlers to mutate message state safely. External callers should
    // avoid concurrent mutation unless they add their own synchronization.
    struct TBX_API Message
    {
        Message();
        virtual ~Message();

        MessageState state = MessageState::InProgress;
        std::any payload = {};
        Result result = {};
        TimeSpan timeout = {};
        TimeSpan delay_in_seconds = {};
        uint64 delay_in_ticks = 0;
        CancellationToken cancellation_token = {};
        MessageCallbacks callbacks = {};
        bool require_handling = false;
        Uuid id = Uuid::generate();
    };

    // Simple event message with no response.
    struct TBX_API Event : public Message
    {
        Event() = default;
        virtual ~Event() = default;
    };

    // Request message with a typed response.
    template <typename T>
    struct Request : public Message
    {
        Request() = default;
        virtual ~Request() = default;

        T result = {};
    };

    // Void specialization to allow requests without a result payload.
    template <>
    struct Request<void> : public Message
    {
        Request() = default;
        virtual ~Request() = default;
    };

    // Subscriber callback invoked by the message coordinator during dispatch.
    // Ownership: non-owning. The coordinator stores a copy of the callable.
    // Thread-safety: Invoked on the coordinator's calling thread; handlers
    // should avoid blocking and must manage their own synchronization if
    // touching shared state.
    using MessageHandler = std::function<void(Message&)>;

    // Helper for invoking a handler only when the message matches the expected type.
    // Ownership: Non-owning; the callback is invoked directly and not retained beyond
    // the call. Captured resources must outlive the invocation.
    // Thread-safety: Matches the caller's context. No synchronization is applied.
    template <typename THandler>
    bool on_message(const Message& message, THandler&& handler)
    {
        // This all allows us to call on_message with lambdas like:
        //  on_message(msg, [](const MyMessageType& m) { ... }); instead of
        // on_message<MyMessageType>(msg, [](const MyMessageType& m) { ... });
        using Handler = std::decay_t<THandler>;
        using MemberPtr = decltype(&Handler::operator());
        using Arg = typename FunctionTraits<MemberPtr>::ArgType;
        using MessageType = std::remove_cv_t<std::remove_reference_t<Arg>>;

        if (const auto* typed = dynamic_cast<const MessageType*>(&message))
        {
            // Arg is e.g. "const MyMessageType&" or "MyMessageType&"
            handler(static_cast<Arg>(*typed));
            return true;
        }

        return false;
    }

    // Helper for invoking a handler only when the message matches the expected type.
    // Ownership: Non-owning; the callback is invoked directly and not retained beyond
    // the call. Captured resources must outlive the invocation.
    // Thread-safety: Matches the caller's context. No synchronization is applied.
    template <typename THandler>
    bool on_message(Message& message, THandler&& handler)
    {
        // This all allows us to call on_message with lambdas like:
        //  on_message(msg, [](const MyMessageType& m) { ... }); instead of
        // on_message<MyMessageType>(msg, [](const MyMessageType& m) { ... });
        using Handler = std::decay_t<THandler>;
        using MemberPtr = decltype(&Handler::operator());
        using Arg = typename FunctionTraits<MemberPtr>::ArgType;
        using MessageType = std::remove_cv_t<std::remove_reference_t<Arg>>;

        if (auto* typed = dynamic_cast<MessageType*>(&message))
        {
            // Arg is e.g. "const MyMessageType&" or "MyMessageType&"
            handler(static_cast<Arg>(*typed));
            return true;
        }

        return false;
    }
}
