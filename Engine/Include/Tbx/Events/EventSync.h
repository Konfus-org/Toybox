#pragma once
#include <mutex>

namespace Tbx
{
    /// <summary>
    /// Guards access to the shared event bus state via a single static mutex.
    /// The mutex is locked for the lifetime of the instance.
    /// </summary>
    class EventSync
    {
    public:
        EventSync();
        ~EventSync();

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

inline EventSync::EventSync()
    : _lock(Mutex())
{
}

inline EventSync::~EventSync() = default;

inline std::mutex& EventSync::Mutex()
{
    static std::mutex mutex;
    return mutex;
}

