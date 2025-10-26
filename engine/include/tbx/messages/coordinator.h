#pragma once
#include "tbx/messages/dispatcher.h"
#include "tbx/messages/handler.h"
#include "tbx/memory/smart_pointers.h"
#include "tbx/ids/uuid.h"
#include <utility>
#include <vector>

namespace tbx
{
    // Concrete coordinator that:
    //  - Tracks subscribers (IMessageHandler*) and delivers messages via send()
    //  - Owns a queue of copies for deferred delivery via post()/process()
    // This type implements both the dispatch interface (for producers)
    // and the processor interface (for the engine/application loop).
    class MessageCoordinator : public IMessageDispatcher, public IMessageProcessor
    {
    public:
        // Subscription management
        Uuid add_handler(IMessageHandler* handler);
        Uuid add_handler(IMessageHandler& handler);
        void remove_handler(const Uuid& token);
        void clear();

        // IMessageDispatcher
        void send(const Message& msg) const override;
        // Copies the message for deferred processing
        void post(const Message& msg) override;

        // IMessageProcessor
        void process() override;

    private:
        std::vector<std::pair<Uuid, IMessageHandler*>> _handlers;
        std::vector<Scope<Message>> _pending;
        std::vector<Scope<Message>> _processing;
    };
}
