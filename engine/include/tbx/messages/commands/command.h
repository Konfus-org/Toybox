#pragma once
#include "tbx/messages/message.h"

namespace tbx
{
    // Base class for all commands.
    // A command is a message that should be acted on.
    class Command : public Message
    {
    public:
        ~Command() override = default;
    };
}
