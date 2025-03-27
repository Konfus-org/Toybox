#include "TbxPch.h"
#include "Events.h"

namespace Tbx 
{
    std::unordered_map<hash, std::vector<Callback<Event>>> Events::_subscribers;
    std::mutex Events::_mutex;

    void Events::Clear()
    {
        _subscribers.clear();
    }

    std::mutex& Events::GetMutex()
    {
        return _mutex;
    }

    std::unordered_map<hash, std::vector<Callback<Event>>>& Events::GetSubscribers()
    {
        return _subscribers;
    }
}