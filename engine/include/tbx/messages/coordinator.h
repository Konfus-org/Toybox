#pragma once
#include "tbx/messages/dispatcher.h"
#include "tbx/messages/handler.h"
#include "tbx/memory/smart_pointers.h"
#include "tbx/ids/uuid.h"
#include <chrono>
#include <utility>
#include <vector>

namespace tbx
{
    // Concrete coordinator that:
    //  - Tracks subscribers (MessageHandler callbacks) and delivers messages via send()
    //  - Owns a queue of copies for deferred delivery via post()/process()
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
        MessageResult send(const Message& msg, const MessageConfiguration& config = {}) const override;
        // Copies the message for deferred processing
        MessageResult post(const Message& msg, const MessageConfiguration& config = {}) override;

        // IMessageProcessor
        void process() override;

    private:
        struct QueuedMessage
        {
            Scope<Message> message;
            MessageConfiguration config;
            MessageResult result;
            std::size_t remaining_ticks = 0;
            bool has_tick_delay = false;
            bool has_time_delay = false;
            std::chrono::steady_clock::time_point ready_time{};
        };

        MessageResult dispatch(Message& msg, const MessageConfiguration& config, MessageResult result) const;
        void finalize_callbacks(const Message& msg, const MessageConfiguration& config, MessageResult& result, MessageStatus status) const;

        std::vector<std::pair<Uuid, MessageHandler>> _handlers;
        std::vector<QueuedMessage> _pending;
        std::vector<QueuedMessage> _processing;
    };
}
