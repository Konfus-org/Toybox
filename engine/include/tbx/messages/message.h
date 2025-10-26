#pragma once
#include "tbx/ids/uuid.h"

namespace tbx
{
    // Base polymorphic message type for dispatching.
    struct Message
    {
        virtual ~Message() = default;

        Uuid id = Uuid::generate();
        bool is_handled = false;

        template <typename TMessage> requires std::is_base_of_v<Message, TMessage>
        bool is() const
        {
            return dynamic_cast<const TMessage*>(this) != nullptr;
        }

        template <typename TMessage> requires std::is_base_of_v<Message, TMessage>
        const TMessage& as() const {
            const TMessage* derived = dynamic_cast<const TMessage*>(this);
            if (!derived) {
                throw std::bad_cast();
            }
            return *derived;
        }

        template <typename TMessage> requires std::is_base_of_v<Message, TMessage>
        bool try_as(TMessage* out) const
        {
            const TMessage* derived = dynamic_cast<const TMessage*>(this);
            if (!derived)
            {
                return false;
            }
            *out = *derived;
            return true;
        }
    };
}

