#pragma once
#include <functional>
#include <utility>
#include <vector>
#include <memory>
#include <type_traits>
#include "tbx/ids/uuid.h"
#include "tbx/messages/message.h"

namespace tbx
{
    // Generic, type-erased dispatcher for a message type T.
    class MessageDispatcher
    {
    public:
        using Handler = std::function<void(const Message&)>;

        // Adds a handler and returns a token for removal.
        Uuid add_handler(Handler handler)
        {
            Uuid id = Uuid::generate();
            _handlers.emplace_back(id, std::move(handler));
            return id;
        }

        // Removes a handler by token. No-op if not found.
        void remove_handler(const Uuid& token)
        {
            std::vector<std::pair<Uuid, Handler>> next;
            next.reserve(_handlers.size());
            for (auto& entry : _handlers)
            {
                if (!(entry.first == token))
                {
                    next.emplace_back(std::move(entry));
                }
            }
            _handlers.swap(next);
        }

        // Clears all handlers.
        void clear()
        {
            _handlers.clear();
            _pending.clear();
            _processing.clear();
        }

        // Executes immediately
        void send(const Message& msg) const
        {
            for (const auto& entry : _handlers)
            {
                if (msg.is_handled)
                    break;
                entry.second(msg);
            }
        }

        // Posts a copy of the message for delivery on next process()
        template <typename TMessage>
        void post(const TMessage& msg)
        {
            static_assert(std::is_base_of<Message, TMessage>::value, "post<T> requires T : Message");
            _pending.emplace_back(new TMessage(msg));
        }

        // Processes posted messages from the previous frame and delivers them via send().
        void process()
        {
            _processing.clear();
            _processing.swap(_pending);
            for (const auto& ptr : _processing)
            {
                if (!ptr) continue;
                send(*ptr);
            }
        }

    private:
        std::vector<std::pair<Uuid, Handler>> _handlers;
        std::vector<std::unique_ptr<Message>> _pending;
        std::vector<std::unique_ptr<Message>> _processing;
    };

}
