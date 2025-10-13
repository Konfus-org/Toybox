#pragma once
#include "Tbx/DllExport.h"
#include <mutex>

namespace Tbx
{
    /// <summary>
    /// Guards access to the shared event bus state via a single static mutex.
    /// The mutex is locked for the lifetime of the instance.
    /// </summary>
    class TBX_EXPORT EventSync
    {
    public:
        EventSync();
        ~EventSync() = default;

        EventSync(const EventSync&) = delete;
        EventSync& operator=(const EventSync&) = delete;
        EventSync(EventSync&&) = delete;
        EventSync& operator=(EventSync&&) = delete;

        /// <summary>
        /// Provides access to the shared event mutex.
        /// </summary>
        static std::mutex& Mutex();

    private:
        std::unique_lock<std::mutex> _lock;
    };
}
