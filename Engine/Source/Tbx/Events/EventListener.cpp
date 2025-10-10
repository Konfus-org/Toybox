#include "Tbx/PCH.h"
#include "Tbx/Events/EventListener.h"
#include <vector>

namespace Tbx
{
    EventListener::EventListener(WeakRef<EventBus> bus)
    {
        Bind(bus);
    }

    EventListener::~EventListener()
    {
        Unbind();
    }

    void EventListener::Bind(WeakRef<EventBus> bus)
    {
        TBX_ASSERT(bus.lock(), "EventListener: Cannot bind to a null event bus reference.");

        Unbind();

        auto current = _bus.lock();
        auto next = bus.lock();
        if (current && next && current == next)
        {
            return;
        }

        _bus = bus;
    }

    void EventListener::Unbind()
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
            else
            {
                TBX_ASSERT(false, "EventListener: Encountered an invalid subscription token during unbind.");
            }
        }

        _bus.reset();
    }

    bool EventListener::IsBound() const
    {
        return !_bus.expired();
    }

    void EventListener::StopListening(const Uid& token)
    {
        if (token == Uid::Invalid)
        {
            TBX_ASSERT(false, "EventListener: Cannot stop listening with an invalid subscription token.");
            return;
        }

        {
            std::scoped_lock lock(_mutex);
            _activeTokens.erase(token);
        }

        auto bus = LockBus();
        if (!bus)
        {
            return;
        }

        bus->Unsubscribe(token);
    }

    void EventListener::TrackToken(const Uid& token)
    {
        if (token == Uid::Invalid)
        {
            TBX_ASSERT(false, "EventListener: Cannot track an invalid subscription token.");
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