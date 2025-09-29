#include "Tbx/PCH.h"
#include "Tbx/Events/EventListener.h"
#include <vector>

namespace Tbx
{
    EventListener::EventListener(const Ref<EventBus>& bus)
    {
        Bind(bus);
    }

    EventListener::EventListener(const WeakRef<EventBus>& bus)
    {
        Bind(bus);
    }

    EventListener::~EventListener()
    {
        StopListening();
    }

    void EventListener::Bind(const Ref<EventBus>& bus)
    {
        TBX_ASSERT(bus, "EventListener: Cannot bind to a null event bus reference.");
        Bind(WeakRef<EventBus>(bus));
    }

    void EventListener::Bind(const WeakRef<EventBus>& bus)
    {
        auto current = _bus.lock();
        auto next = bus.lock();
        if (current && next && current == next)
        {
            return;
        }

        StopListening();
        _bus = bus;
    }

    void EventListener::Unbind()
    {
        StopListening();
        _bus.reset();
    }

    void EventListener::StopListening()
    {
        std::vector<Uid> tokens;
        {
            std::scoped_lock lock(_mutex);
            tokens.assign(_activeTokens.begin(), _activeTokens.end());
            _activeTokens.clear();
        }

        auto bus = LockBus();
        if (!bus)
        {
            return;
        }

        for (const auto& token : tokens)
        {
            if (token != Uid::Invalid)
            {
                bus->Unsubscribe(token);
            }
        }
    }

    bool EventListener::IsBound() const
    {
        return !_bus.expired();
    }

    void EventListener::TrackToken(const Uid& token)
    {
        if (token == Uid::Invalid)
        {
            return;
        }

        std::scoped_lock lock(_mutex);
        _activeTokens.insert(token);
    }

    Ref<EventBus> EventListener::LockBus() const
    {
        return _bus.lock();
    }
}

