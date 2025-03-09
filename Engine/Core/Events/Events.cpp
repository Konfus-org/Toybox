#include "TbxPch.h"
#include "Events.h"

namespace Tbx 
{
    std::unordered_map<hash, std::vector<Callback<Event>>> Events::_subscribers;
    std::mutex Events::_mutex;

    TBX_API std::mutex& Events::GetMutex()
    {
        return _mutex;
    }

    TBX_API std::unordered_map<hash, std::vector<Callback<Event>>>& Events::GetSubscribers()
    {
        return _subscribers;
    }
}