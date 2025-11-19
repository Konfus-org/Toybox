#pragma once
#include "tbx/messages/message.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    // Base class for events.
    // Events are a fire and forget message, think of them like notifications.
    struct TBX_API Event : public Message
    {
        Event();
        virtual ~Event();
    };
}
