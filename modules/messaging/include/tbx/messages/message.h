#pragma once
#include "tbx/async/cancellation_token.h"
#include "tbx/common/result.h"
#include "tbx/common/uuid.h"
#include <functional>

namespace tbx
{
    enum class MessageState
    {
        UN_HANDLED,
        HANDLED,
        CANCELLED,
        ERROR
    };

    enum class MessageNotHandledBehavior
    {
        DO_NOTHING,
        WARN,
        ASSERT,
    };

    struct Message;

    struct TBX_API MessageCallbacks
    {
        std::function<void(const Message&)> on_error;
        std::function<void(const Message&)> on_cancelled;
        std::function<void(const Message&)> on_processed;
        std::function<void(const Message&)> on_timeout;
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
        virtual ~Message() noexcept;

        MessageState state = MessageState::UN_HANDLED;
        Result result = {};
        CancellationToken cancellation_token = {};
        MessageCallbacks callbacks = {};
        Uuid id = Uuid::generate();
    };

    // Simple event message with no response.
    struct TBX_API Event : public Message
    {
        Event() = default;
        virtual ~Event() noexcept = default;
    };

    struct TBX_API RequestBase : public Message
    {
        RequestBase() = default;
        virtual ~RequestBase() noexcept = default;

        MessageNotHandledBehavior not_handled_behavior = MessageNotHandledBehavior::DO_NOTHING;
    };

    // Request message with a typed response.
    template <typename T>
    struct Request : public RequestBase
    {
        Request() = default;
        virtual ~Request() noexcept = default;

        T result = {};
    };

    // Void specialization to allow requests without a result payload.
    template <>
    struct Request<void> : public RequestBase
    {
        Request() = default;
        virtual ~Request() noexcept = default;
    };

    /// <summary>
    /// Purpose: Represents a subscriber callback invoked by the message coordinator during
    /// dispatch.
    /// </summary>
    /// <remarks>
    /// Ownership: Non-owning. The coordinator stores a copy of the callable.
    /// Thread Safety: Invoked on the coordinator's calling thread; handlers should avoid blocking
    /// and must manage their own synchronization if touching shared state.
    /// </remarks>
    using MessageHandler = std::function<void(Message&)>;

    /// <summary>
    /// Purpose: Retrieves a typed message pointer from a const base message reference.
    /// </summary>
    /// <remarks>
    /// Ownership: Non-owning; the returned pointer borrows from the input message.
    /// Thread Safety: Matches the caller's context. No synchronization is applied.
    /// </remarks>
    template <typename TMessage>
    const TMessage* handle_message(const Message& message)
    {
        return dynamic_cast<const TMessage*>(&message);
    }

    /// <summary>
    /// Purpose: Retrieves a typed message pointer from a mutable base message reference.
    /// </summary>
    /// <remarks>
    /// Ownership: Non-owning; the returned pointer borrows from the input message.
    /// Thread Safety: Matches the caller's context. No synchronization is applied.
    /// </remarks>
    template <typename TMessage>
    TMessage* handle_message(Message& message)
    {
        return dynamic_cast<TMessage*>(&message);
    }
}
