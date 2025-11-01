#pragma once
#include "tbx/messages/message.h"
#include "tbx/state/result.h"

namespace tbx
{
    // Interface for components that can dispatch messages to the system.
    // Implementations decide how messages are routed; this is a minimal
    // surface for producers to send immediately or queue for later.
    class IMessageDispatcher
    {
       public:
        virtual ~IMessageDispatcher() = default;
        // Immediately send message on the stack.
        virtual Result send(const Message& msg) const = 0;
        // Copies the message for deferred processing on the next process() tick.
        virtual Result post(const Message& msg) = 0;
    };

    // Interface for components that advance queued work and deliver
    // posted messages. Typically driven once per frame/tick.
    class IMessageProcessor
    {
       public:
        virtual ~IMessageProcessor() = default;

        // processes all posted messages.
        virtual void process() = 0;
    };
}
