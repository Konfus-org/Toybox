#include "Tbx/PCH.h"
#include "Tbx/Events/EventListener.h"
#include <vector>

namespace Tbx
{
    EventListener::EventListener(Ref<EventBus> bus)
    {
        Bind(bus);
    }

    EventListener::EventListener(EventListener&& other) noexcept
    {
        Transfer(other, *this);
    }

    EventListener& EventListener::operator=(EventListener&& other) noexcept
    {
        if (this != &other)
        {
            ReleaseSubscriptions();
            Transfer(other, *this);
        }
        return *this;
    }

    EventListener::~EventListener()
    {
        Unbind();
    }

    void EventListener::Bind(Ref<EventBus> bus)
    {
        TBX_ASSERT(bus, "EventListener: Cannot bind to a null event bus reference.");

        if (_bus == bus)
        {
            return;
        }

        Unbind();

        _bus = bus;
    }

    void EventListener::ReleaseSubscriptions()
    {
        std::vector<Uid> tokens;
        {
            std::scoped_lock lock(_mutex);
            tokens.assign(_activeTokens.begin(), _activeTokens.end());
            _activeTokens.clear();
        }

        const auto bus = LockBus();
        if (!bus)
        {
            return;
        }

        for (const auto& token : tokens)
        {
            if (token == Uid::Invalid)
            {
                TBX_ASSERT(false, "EventListener: Encountered an invalid subscription token during unbind.");
                continue;
            }

            bus->RemoveSubscription(token);
        }
    }

    void EventListener::Unbind()
    {
        ReleaseSubscriptions();

        _bus = nullptr;
    }

    bool EventListener::IsBound() const
    {
        return _bus != nullptr;
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

        const auto bus = LockBus();
        if (bus)
        {
            bus->RemoveSubscription(token);
        }
    }

    void EventListener::Transfer(EventListener& from, EventListener& to) noexcept
    {
        if (&from == &to)
        {
            return;
        }

        Ref<EventBus> bus;
        std::unordered_set<Uid> tokens;

        {
            std::scoped_lock lock(from._mutex);
            tokens = std::move(from._activeTokens);
            bus = std::move(from._bus);
        }

        {
            std::scoped_lock lock(to._mutex);
            to._activeTokens = std::move(tokens);
        }

        to._bus = std::move(bus);
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
        return _bus;
    }
}
