#include "Tbx/Events/EventSync.h"

namespace Tbx
{
    EventSync::EventSync()
        : _lock(Mutex())
    {
    }

    std::mutex& EventSync::Mutex()
    {
        static std::mutex mutex;
        return mutex;
    }
}
