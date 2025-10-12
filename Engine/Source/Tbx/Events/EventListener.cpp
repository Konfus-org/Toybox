#include "Tbx/PCH.h"
#include "Tbx/Events/EventListener.h"
#include <vector>

namespace Tbx
{
    EventListener::EventListener(Ref<EventBus> bus)
    {
        Bind(bus);
    }

    EventListener::EventListener(const EventListener& other)
    {
        Transfer(const_cast<EventListener&>(other), *this);
    }

    EventListener& EventListener::operator=(const EventListener& other)
    {
        if (this != &other)
        {
            ReleaseSubscriptions();
            Transfer(const_cast<EventListener&>(other), *this);
        }
        return *this;
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

        if (_bus && bus && _bus == bus)
        {
            return;
        }

        Unbind();

        _bus = bus;
    }

    namespace
    {
        void RemoveSubscription(Ref<EventBus> bus, const Uid& token)
        {
            if (!bus || token == Uid::Invalid)
            {
                return;
            }

            EventSync sync;

            auto indexIt = bus->SubscriptionIndex.find(token);
            if (indexIt == bus->SubscriptionIndex.end())
            {
                return;
            }

            const auto eventKey = indexIt->second;
            auto eventIt = bus->Subscriptions.find(eventKey);
            if (eventIt != bus->Subscriptions.end())
            {
                auto& callbacks = eventIt->second;
                callbacks.erase(token);
                if (callbacks.empty())
                {
                    bus->Subscriptions.erase(eventIt);
                }
            }

            bus->SubscriptionIndex.erase(indexIt);
        }
    }

    void EventListener::ReleaseSubscriptions()
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
                RemoveSubscription(bus, token);
            }
            else
            {
                TBX_ASSERT(false, "EventListener: Encountered an invalid subscription token during unbind.");
            }
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

        auto bus = LockBus();
        if (!bus)
        {
            std::scoped_lock lock(_mutex);
            _activeTokens.erase(token);
            return;
        }

        {
            std::scoped_lock lock(_mutex);
            _activeTokens.erase(token);
        }

        RemoveSubscription(bus, token);
    }

    void EventListener::Transfer(EventListener& from, EventListener& to) noexcept
    {
        if (&from == &to)
        {
            return;
        }

        Ref<EventBus> busCopy;
        std::unordered_set<Uid> tokens;

        {
            std::scoped_lock lock(from._mutex);
            tokens = std::move(from._activeTokens);
            from._activeTokens.clear();
            busCopy = from._bus;
            from._bus = nullptr;
        }

        {
            std::scoped_lock lock(to._mutex);
            to._activeTokens = std::move(tokens);
        }

        to._bus = busCopy;
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
