#pragma once
#include "tbx/messages/dispatcher.h"
#include "tbx/messages/handler.h"
#include "tbx/memory/smart_pointers.h"
#include "tbx/ids/uuid.h"
#include "tbx/time/timer.h"
#include <chrono>
#include <string>
#include <utility>
#include <vector>

namespace tbx
{
    struct QueuedMessage
    {
        Scope<Message> message;
        Result result;
        Timer timer;
    };

    // Concrete coordinator that (not thread-safe; callers serialize access):
    //  - Tracks subscribers (MessageHandler callbacks) and delivers messages via send()
    //  - Owns queue storage for deferred delivery via post()/process()
    // This type implements both the dispatch interface (for producers)
    // and the processor interface (for the engine/application loop).
    class MessageCoordinator : public IMessageDispatcher, public IMessageProcessor
    {
    public:
        // Subscription management
        Uuid add_handler(MessageHandler handler);
        void remove_handler(const Uuid& token);
        void clear();

        // IMessageDispatcher
        Result send(const Message& msg) const override;
        // Copies the message for deferred processing
        Result post(const Message& msg) override;

        // IMessageProcessor
        void process() override;

    private:
        void dispatch(Message& msg, Result& result) const;
        void finalize_callbacks(const Message& msg, Result& result, ResultStatus status, const std::string* failure_reason = nullptr) const;

        std::vector<std::pair<Uuid, MessageHandler>> _handlers;
        std::vector<QueuedMessage> _pending;
        std::vector<QueuedMessage> _processing;
    };
}
