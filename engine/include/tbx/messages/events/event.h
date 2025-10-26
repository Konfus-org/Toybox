#pragma once
#include "tbx/messages/message.h"

namespace tbx
{
    // Base class for events.
    // Events are a fire and forget message, think of them like notifications.
    class Event : public Message
    {
    public:
        ~Event() override = default;
    };
}
