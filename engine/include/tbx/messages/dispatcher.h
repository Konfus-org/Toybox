#pragma once
#include "tbx/messages/message_configuration.h"
#include "tbx/messages/message_result.h"
#include <type_traits>

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
        virtual MessageResult send(const Message& msg, const MessageConfiguration& config = {}) const = 0;
        // Copies the message for deferred processing on the next process() tick.
        virtual MessageResult post(const Message& msg, const MessageConfiguration& config = {}) = 0;
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
