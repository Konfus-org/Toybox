#pragma once
#include "tbx/messages/message.h"

namespace tbx
{
    class Event : public Message
    {
    public:
        ~Event() override = default;
    };
}
