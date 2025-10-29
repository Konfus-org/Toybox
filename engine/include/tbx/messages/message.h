#pragma once
#include "tbx/ids/uuid.h"
#include "tbx/state/cancellation_token.h"
#include <functional>
#include <optional>

namespace tbx
{
    class MessageResult;

    // Base polymorphic message type for dispatching.
    struct Message
    {
        virtual ~Message() = default;

        Uuid id = Uuid::generate();
        bool is_handled = false;

        CancellationToken cancellation_token;

        // Non-owning result handle managed by the coordinator; not thread-safe.
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

