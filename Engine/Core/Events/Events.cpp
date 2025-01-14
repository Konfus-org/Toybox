#include "TbxPCH.h"
#include "Events.h"

namespace Tbx
{
    std::unordered_map<hash, std::vector<Callback<Event>>> Events::_subscribers;
    std::mutex Events::_mutex;

    template<class TEvent>
    UUID Events::Subscribe(const CallbackFunction<TEvent>& callback)
    {
        std::scoped_lock<std::mutex> lock(_mutex);

        const auto& eventInfo = typeid(TEvent);
        const auto& hashCode = eventInfo.hash_code();

        if (_subscribers.contains(hashCode) == false)
        {
            _subscribers[hashCode] = std::vector<Callback<Event>>();
        }

        auto& callbacks = _subscribers[hashCode];
        auto callbackToAdd = Callback<Event>([callback](Event& event) { callback(static_cast<TEvent&>(event)); });
        callbacks.push_back(callbackToAdd);

        return callbackToAdd.GetId();
    }

    template<class TEvent>
    void Events::Unsubscribe(const UUID& callbackToUnsub)
    {
        std::scoped_lock<std::mutex> lock(_mutex);

        const auto& eventInfo = typeid(TEvent);
        const auto& hashCode = eventInfo.hash_code();

        if (_subscribers.contains(hashCode) == false)
        {
            return;
        }

        auto& callbacks = _subscribers[hashCode];
        for (auto it = callbacks.begin(); it != callbacks.end();)
        {
            if (it->GetId() == callbackToUnsub)
            {
                it = callbacks.erase(it);
                break;
            }
            it++;
        }

        if (callbacks.empty())
        {
            _subscribers.erase(hashCode);
        }
    }

    template <class TEvent>
    void Events::Send(TEvent& event)

    {
        std::scoped_lock<std::mutex> lock(_mutex);

        const auto& eventInfo = typeid(TEvent);
        const auto& hashCode = eventInfo.hash_code();

        if (_subscribers.contains(hashCode) == false)
        {
            return;
        }

        const auto& callbacks = _subscribers[hashCode];
        for (auto& callback : callbacks)
        {
            callback(event);
        }
    }
}