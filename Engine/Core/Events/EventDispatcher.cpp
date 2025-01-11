#include "TbxPCH.h"
#include "EventDispatcher.h"

namespace Tbx
{
    std::unordered_map<uint64, std::vector<std::function<void(Event&)>>> EventDispatcher::_subscribers;
    std::mutex EventDispatcher::_mutex;

    template<typename EventType>
    void EventDispatcher::Subscribe(const std::function<void(EventType&)>& callback)
    {
        std::scoped_lock<std::mutex> lock(_mutex);

        const auto& eventInfo = typeid(EventType);
        const auto& hashCode = eventInfo.hash_code();
        const auto& name = eventInfo.name();
        TBX_INFO("The event {0}-{1} has been subscribed to!", name, hashCode);

        if (_subscribers.contains(hashCode) == false)
        {
            _subscribers[hashCode] = std::vector<std::function<void(Event&)>>();
        }

        auto& callbacks = _subscribers[hashCode];
        callbacks.push_back([callback](Event& event)
            {
                callback(static_cast<EventType&>(event));
            });
    }

    template<typename EventType>
    void EventDispatcher::Unsubscribe(const std::function<void(EventType&)>& callback)
    {
        // TODO: implement
    }

    template<typename EventType>
    void EventDispatcher::Send(EventType& event)
    {
        std::scoped_lock<std::mutex> lock(_mutex);

        const auto& eventInfo = typeid(EventType);
        const auto& hashCode = eventInfo.hash_code();
        const auto& name = eventInfo.name();
        TBX_INFO("The event {0}-{1} has been sent!", name, hashCode);

        if (_subscribers.contains(hashCode) == false)
        {
            return;
        }

        const auto& callbacks = _subscribers[hashCode];
        for (const auto& callback : callbacks)
        {
            callback(event);
            if (event.Handled)
            {
                break;
            }
        }
    }
}