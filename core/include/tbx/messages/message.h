#pragma once
#include "tbx/ids/uuid.h"
#include "tbx/state/cancellation_token.h"
#include "tbx/state/result.h"
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
        std::any payload = nullptr;
        Result result = {};
        TimeSpan timeout = {};
        TimeSpan delay_in_seconds = {};
        std::size_t delay_in_ticks = 0;
        CancellationToken cancellation_token = {};
        MessageCallbacks callbacks = {};
        Uuid id = Uuid::generate();
    };
}
