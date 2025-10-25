#pragma once
#include "tbx/messages/message.h"

namespace tbx
{
    // Base command type for high-frequency dispatch.
    // Concrete commands derive from this and carry their own data.
    class Command : public Message
    {
    public:
        ~Command() override = default;
    };
}
