#include "tbx/messages/coordinator.h"
#include "tbx/memory/smart_pointers.h"

namespace tbx
{
    Uuid MessageCoordinator::add_handler(IMessageHandler* handler)
    {
        return add_handler(*handler);
    }

    Uuid MessageCoordinator::add_handler(IMessageHandler& handler)
    {
        Uuid id = Uuid::generate();
        _handlers.emplace_back(id, &handler);
        return id;
    }

    void MessageCoordinator::remove_handler(const Uuid& token)
    {
        std::vector<std::pair<Uuid, IMessageHandler*>> next;
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

    void MessageCoordinator::clear()
    {
        _handlers.clear();
        _pending.clear();
        _processing.clear();
    }

    void MessageCoordinator::send(const Message& msg) const
    {
        for (const auto& entry : _handlers)
        {
            if (msg.is_handled)
                break;
            if (entry.second)
                entry.second->on_message(msg);
        }
    }

    void MessageCoordinator::post(const Message& msg)
    {
        // Create a shallow copy of the polymorphic Message base. Derived
        // types should only carry data in the base part or be trivially
        // copyable into the base slice for deferred dispatch.
        struct Copy final : Message { Copy(const Message& m) { *static_cast<Message*>(this) = m; } };
        _pending.emplace_back(make_scope<Copy>(msg));
    }

    void MessageCoordinator::process()
    {
        _processing.clear();
        _processing.swap(_pending);
        for (const auto& ptr : _processing)
        {
            if (!ptr) continue;
            send(*ptr);
        }
    }
}
