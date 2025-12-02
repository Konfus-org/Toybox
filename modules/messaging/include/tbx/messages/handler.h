#pragma once
#include "tbx/messages/message.h"
#include <functional>

namespace tbx
{
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
    template <typename TMessage, typename THandler>
    bool handle_message(Message& message, THandler&& handler)
    {
        if (auto* typed_message = dynamic_cast<TMessage*>(&message))
        {
            handler(*typed_message);
            return true;
        }

        return false;
    }

    template <typename TMessage, typename THandler>
    bool handle_message(const Message& message, THandler&& handler)
    {
        if (auto* typed_message = dynamic_cast<const TMessage*>(&message))
        {
            handler(*typed_message);
            return true;
        }

        return false;
    }
}
