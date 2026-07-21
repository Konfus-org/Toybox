#pragma once
#include "tbx/core/typedefs.h"
#include <cstring>
#include <mutex>
#include <vector>

namespace tbx
{
    using Token = uint64;

    /// @brief
    /// Purpose: One queued-event record: which signal to dispatch through and where its payload
    /// lives in the frame arena.
    struct QueuedEvent
    {
        void (*dispatch)(void* signal, const void* payload) = nullptr;
        void* signal = nullptr;
        size offset = 0;
    };

    /// @brief
    /// Purpose: Per-frame linear arena of trivially-copyable events.
    /// @details
    /// Ownership: Owned by Events. Thread Safety: push() is safe from any thread; drain() runs
    /// on the main thread once per frame (Engine::pump()). Events emitted during a drain land
    /// in the next frame — deterministic, no re-entrancy.
    class EventQueue final
    {
      public:
        /// @brief
        /// Purpose: Appends one event payload for delivery at the next drain().
        template <typename TEvent>
        void push(void* signal, void (*dispatch)(void*, const void*), const TEvent& event)
        {
            static_assert(std::is_trivially_copyable_v<TEvent>);
            std::scoped_lock lock(_mutex);
            const size offset = align_up(_bytes.size(), alignof(TEvent));
            _bytes.resize(offset + sizeof(TEvent));
            std::memcpy(_bytes.data() + offset, &event, sizeof(TEvent));
            _entries.push_back({.dispatch = dispatch, .signal = signal, .offset = offset});
        }

        /// @brief
        /// Purpose: Dispatches everything queued since the last drain, in emission order.
        void drain()
        {
            {
                std::scoped_lock lock(_mutex);
                _draining_entries.swap(_entries);
                _draining_bytes.swap(_bytes);
            }
            for (const QueuedEvent& entry : _draining_entries)
                entry.dispatch(entry.signal, _draining_bytes.data() + entry.offset);
            _draining_entries.clear();
            _draining_bytes.clear();
        }

      private:
        static size align_up(size value, size alignment)
        {
            return (value + alignment - 1) & ~(alignment - 1);
        }

      private:
        std::mutex _mutex;
        std::vector<std::byte> _bytes;
        std::vector<QueuedEvent> _entries;
        std::vector<std::byte> _draining_bytes;
        std::vector<QueuedEvent> _draining_entries;
    };
}
