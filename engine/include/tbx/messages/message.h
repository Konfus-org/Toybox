#pragma once
#include "tbx/ids/uuid.h"
#include "tbx/state/cancellation_token.h"
#include "tbx/state/result.h"
#include "tbx/time/time_span.h"
#include <functional>
#include <optional>

namespace tbx
{
    using MessageStatus = ResultStatus;
    using MessageResult = Result;

    // Base polymorphic message type for dispatching.
    // Ownership: Messages are typically stack-allocated and passed by reference
    // to send(). When posted, a copy is stored by the coordinator until delivery.
    // The callbacks stored herein are non-owning; ensure captured state outlives
    // dispatch or use weak references.
    // Thread-safety: Not inherently thread-safe. A message instance should only
    // be read/written by one thread at a time. The optional Result handle is
    // a non-owning reference managed by the coordinator.
    struct TBX_API Message
    {
        using Callback = std::function<void(const Message&)>;

        virtual ~Message() = default;

        Uuid id = Uuid::generate();
        bool is_handled = false;

        CancellationToken cancellation_token;

        Callback on_failure;
        Callback on_cancelled;
        Callback on_handled;
        Callback on_processed;

        std::optional<std::size_t> delay_ticks;
        std::optional<TimeSpan> delay_time;

        bool has_delay() const
        {
            return delay_ticks.has_value() || delay_time.has_value();
        }

        bool has_ticks_delay() const
        {
            return delay_ticks.has_value();
        }

        bool has_time_delay() const
        {
            return delay_time.has_value();
        }

        // Non-owning result handle managed by the coordinator.
        // Thread-safety: Access must be externally synchronized if used across threads.
        void set_result(MessageResult& value)
        {
            result_ref = value;
        }

        void clear_result()
        {
            result_ref.reset();
        }

        MessageResult* get_result()
        {
            return result_ref ? &result_ref->get() : nullptr;
        }

        const MessageResult* get_result() const
        {
            return result_ref ? &result_ref->get() : nullptr;
        }

       private:
        std::optional<std::reference_wrapper<MessageResult>> result_ref;
    };
}
