#pragma once
#include "tbx/messages/message.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    // Base class for all commands.
    // A command is a message that should be acted on.
    class TBX_API Command : public Message
    {
      public:
        ~Command() override;
    };
}
