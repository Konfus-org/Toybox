#pragma once
#include "tbx/messages/message.h"

namespace tbx
{
    // Interface for message consumers.
    class IMessageHandler
    {
    public:
        virtual ~IMessageHandler() = default;
        virtual void on_message(const Message& msg) = 0;
    };
}

