#include "TbxPCH.h"
#include "EventDispatcher.h"

namespace Tbx
{
    std::unordered_map<std::type_index, std::vector<EventCallback>> EventDispatcher::_subscriptions;

    template <typename EventType>
    void EventDispatcher::Subscribe(const EventCallback& callback)
    {
        const auto& typeId = std::type_index(typeid(EventType));
        if (!_subscriptions.contains(typeId))
        {
            _subscriptions[typeId] = std::vector<EventCallback>();
        }
        _subscriptions[typeId].push_back(callback);
    }

    template<typename EventType>
    void EventDispatcher::Unsubscribe(const EventCallback& callbackToRemove)
    {
        const auto& typeId = std::type_index(typeid(EventType));
        if (_subscriptions.contains(typeId))
        {
            // TODO: implement unsubscribing...
            
            ////auto callbacks = _subscriptions[typeId];
            ////auto removed = std::remove(callbacks.begin(), callbacks.end(), callbackToRemove);
            ////callbacks.erase(removed, callbacks.end());

            ////if (callbacks.empty())
            ////{
            ////    _subscriptions.erase(typeId);
            ////}
        }
    }

    template<typename EventType>
    void EventDispatcher::Send(const EventType& event)
    {
        const auto& typeId = std::type_index(typeid(event));
        if (_subscriptions.contains(typeId))
        {
            for (const auto& callback : _subscriptions[typeId])
            {
                if (callback(event))
                {
                    return;
                }
            }
        }
    }
}