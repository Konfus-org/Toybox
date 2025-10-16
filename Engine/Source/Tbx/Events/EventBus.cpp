#include "Tbx/PCH.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Debug/Tracers.h"
#include "Tbx/Debug/IPrintable.h"
#include <Tbx/Memory/Refs.h>
#include "Tbx/Memory/Hashing.h"
#include <unordered_map>
#include <algorithm>
#include <vector>

namespace Tbx
{
    //////////// Event Suppressor ///////////////

    std::atomic_int EventSuppressor::_suppressCount = 0;

    EventSuppressor::EventSuppressor()
    {
        Suppress();
    }

    EventSuppressor::~EventSuppressor()
    {
        Unsuppress();
    }

    bool EventSuppressor::IsSuppressing()
    {
        return _suppressCount.load(std::memory_order_relaxed) > 0;
    }

    void EventSuppressor::Suppress()
    {
        _suppressCount.fetch_add(1, std::memory_order_relaxed);
    }

    void EventSuppressor::Unsuppress()
    {
        _suppressCount.fetch_sub(1, std::memory_order_relaxed);
    }

    //////////// Event Bus ///////////////

    Ref<EventBus> EventBus::Global = CreateGlobal();
    bool EventBus::_creatingGlobal = false;

    EventBus::EventBus(Ref<EventBus> parent)
    {
        if (_creatingGlobal)
        {
            return;
        }

        Ref<EventBus> resolvedParent = parent;
        if (!resolvedParent)
        {
            resolvedParent = Global;
        }

        AttachToParent(resolvedParent.get());
    }

    EventBus::~EventBus()
    {
        EventBus* adoptiveParent = const_cast<EventBus*>(_parent);
        DetachChildren(adoptiveParent);
        DetachFromParent();

        std::scoped_lock lock(_mutex);
        while (!_eventQueue.empty())
        {
            _eventQueue.pop();
        }
        _subscriptions.clear();
        _subscriptionIndex.clear();
    }

    void EventBus::Flush()
    {
        std::queue<ExclusiveRef<Event>> localQueue;
        {
            std::scoped_lock lock(_mutex);
            localQueue.swap(_eventQueue);
        }

        while (!localQueue.empty())
        {
            auto event = std::move(localQueue.front());
            localQueue.pop();

            if (!event)
            {
                continue;
            }

            // If suppressed globally, skip processing of queued events as well.
            if (EventSuppressor::IsSuppressing())
            {
                TBX_TRACE_WARNING("EventBus: Queued event \"{}\" suppressed", typeid(event).name());
                continue;
            }

            const auto hashCode = Hash(*event);
            auto callbacks = GetCallbacks(hashCode);
            if (callbacks.empty())
            {
                continue;
            }

            for (auto& [id, callback] : callbacks)
            {
                if (EventSuppressor::IsSuppressing())
                {
                    TBX_TRACE_WARNING("EventBus: The event \"{}\" is suppressed during flush", typeid(event).name());
                    break;
                }

                callback(*event);
            }
        }
    }

    std::unordered_map<Uid, EventCallback> EventBus::GetCallbacks(EventHash eventKey) const
    {
        std::unordered_map<Uid, EventCallback> callbacks;
        CollectCallbacks(eventKey, callbacks);
        return callbacks;
    }

    Uid EventBus::AddSubscription(EventHash eventKey, EventCallback callback)
    {
        TBX_ASSERT(callback, "EventBus: Cannot add an empty subscription callback.");

        const auto token = Uid::Generate();
        {
            std::scoped_lock lock(_mutex);
            auto& callbacks = _subscriptions[eventKey];
            callbacks[token] = std::move(callback);
            _subscriptionIndex[token] = eventKey;
        }

        return token;
    }

    void EventBus::RemoveSubscription(const Uid& token)
    {
        if (token == Uid::Invalid)
        {
            TBX_ASSERT(false, "EventBus: Cannot remove an invalid subscription token.");
            return;
        }

        std::scoped_lock lock(_mutex);

        auto index = _subscriptionIndex.find(token);
        if (index == _subscriptionIndex.end())
        {
            return;
        }

        const auto eventKey = index->second;
        auto subscription = _subscriptions.find(eventKey);
        if (subscription != _subscriptions.end())
        {
            auto& callbacks = subscription->second;
            callbacks.erase(token);
            if (callbacks.empty())
            {
                _subscriptions.erase(subscription);
            }
        }

        _subscriptionIndex.erase(index);
    }

    void EventBus::QueueEvent(ExclusiveRef<Event> event)
    {
        if (!event)
        {
            return;
        }

        std::scoped_lock lock(_mutex);
        _eventQueue.emplace(std::move(event));
    }

    uint32 EventBus::PendingEventCount() const
    {
        std::scoped_lock lock(_mutex);
        return _eventQueue.size();
    }

    Ref<EventBus> EventBus::CreateGlobal()
    {
        _creatingGlobal = true;
        auto bus = MakeRef<EventBus>();
        _creatingGlobal = false;
        return bus;
    }

    void EventBus::CollectCallbacks(EventHash eventKey, std::unordered_map<Uid, EventCallback>& callbacks) const
    {
        std::scoped_lock lock(_mutex);
        CollectCallbacksLocked(eventKey, callbacks);
    }

    void EventBus::CollectCallbacksLocked(EventHash eventKey, std::unordered_map<Uid, EventCallback>& callbacks) const
    {
        auto it = _subscriptions.find(eventKey);
        if (it != _subscriptions.end())
        {
            callbacks.insert(it->second.begin(), it->second.end());
        }

        auto itDecorator = _decorators.begin();
        while (itDecorator != _decorators.end())
        {
            const EventBus* decorator = *itDecorator;
            if (!decorator)
            {
                itDecorator = _decorators.erase(itDecorator);
                continue;
            }

            decorator->CollectCallbacksLocked(eventKey, callbacks);
            ++itDecorator;
        }
    }

    void EventBus::AttachToParent(EventBus* parent)
    {
        if (!parent)
        {
            return;
        }

        if (_parent == parent)
        {
            return;
        }

        DetachFromParent();
        _parent = parent;
        parent->RegisterDecorator(this);
    }

    void EventBus::DetachFromParent()
    {
        const EventBus* parent = _parent;
        _parent = nullptr;

        if (!parent)
        {
            return;
        }

        parent->UnregisterDecorator(this);
    }

    void EventBus::DetachChildren(EventBus* adoptiveParent)
    {
        std::vector<EventBus*> children;
        {
            std::scoped_lock lock(_mutex);
            auto it = _decorators.begin();
            while (it != _decorators.end())
            {
                const EventBus* decorator = *it;
                if (!decorator)
                {
                    it = _decorators.erase(it);
                    continue;
                }

                children.emplace_back(const_cast<EventBus*>(decorator));
                it = _decorators.erase(it);
            }
        }

        for (auto* child : children)
        {
            if (!child)
            {
                continue;
            }

            EventBus* newParent = adoptiveParent;
            if (!newParent)
            {
                newParent = Global.get();
            }

            child->AttachToParent(newParent);
        }
    }

    void EventBus::RegisterDecorator(const EventBus* decorator) const
    {
        if (!decorator)
        {
            return;
        }

        std::scoped_lock lock(_mutex);
        auto it = _decorators.begin();
        while (it != _decorators.end())
        {
            const EventBus* existing = *it;
            if (!existing)
            {
                it = _decorators.erase(it);
                continue;
            }

            if (existing == decorator)
            {
                return;
            }

            ++it;
        }

        _decorators.emplace_back(decorator);
    }

    void EventBus::UnregisterDecorator(const EventBus* decorator) const
    {
        std::scoped_lock lock(_mutex);
        _decorators.erase(std::remove_if(_decorators.begin(), _decorators.end(), [decorator](const EventBus* existing)
        {
            return existing == nullptr || existing == decorator;
        }), _decorators.end());
    }
}

