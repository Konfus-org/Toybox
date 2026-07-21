#pragma once
#include "tbx/core/typedefs.h"
#include "tbx/core/uuid.h"
#include "tbx/platform/keys.h"
#include <cstring>
#include <functional>
#include <mutex>
#include <type_traits>
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

    /// @brief
    /// Purpose: One subscriber slot on a Signal; a null fn is a tombstone swept after dispatch.
    template <typename TEvent>
    struct SignalSubscriber
    {
        Token token = 0;
        void* owner = nullptr;
        std::function<void(const TEvent&)> fn;
    };

    /// @brief
    /// Purpose: A single event kind, declared as a named member of Events — adding an event
    /// means adding a member, deliberately.
    /// @details
    /// Ownership: Subscriptions are owner-tagged so script reloads / the future editor can bulk
    /// purge. Thread Safety: emit() is safe from any thread (it only enqueues); subscribe and
    /// unsubscribe belong to the main thread, where dispatch also runs.
    template <typename TEvent>
    class Signal final
    {
      public:
        explicit Signal(EventQueue& queue)
            : _queue(queue)
        {
        }

      public:
        /// @brief
        /// Purpose: Queues an event for delivery at the next pump drain.
        void emit(const TEvent& event)
        {
            _queue.get().push(this, &Signal::dispatch, event);
        }

        /// @brief
        /// Purpose: Registers a handler; the owner tag groups subscriptions for bulk removal.
        Token subscribe(void* owner, std::function<void(const TEvent&)> fn)
        {
            const Token token = _next_token++;
            _subscribers.push_back({.token = token, .owner = owner, .fn = std::move(fn)});
            return token;
        }

        /// @brief
        /// Purpose: Removes one subscription by its token.
        void unsubscribe(Token token)
        {
            for (auto& subscriber : _subscribers)
                if (subscriber.token == token)
                    subscriber.fn = nullptr;
        }

        /// @brief
        /// Purpose: Removes every subscription registered under the given owner tag.
        void unsubscribe_owner(void* owner)
        {
            for (auto& subscriber : _subscribers)
                if (subscriber.owner == owner)
                    subscriber.fn = nullptr;
        }

      private:
        static void dispatch(void* signal, const void* payload)
        {
            auto& self = *static_cast<Signal*>(signal);
            const auto& event = *static_cast<const TEvent*>(payload);
            // Snapshot the count: handlers subscribed during dispatch see the NEXT event.
            const size count = self._subscribers.size();
            for (size i = 0; i < count; ++i)
                if (self._subscribers[i].fn)
                    self._subscribers[i].fn(event);
            std::erase_if(
                self._subscribers,
                [](const SignalSubscriber<TEvent>& s)
                {
                    return s.fn == nullptr;
                });
        }

      private:
        std::reference_wrapper<EventQueue> _queue;
        std::vector<SignalSubscriber<TEvent>> _subscribers;
        Token _next_token = 1;
    };

    /// @brief
    /// Purpose: Fired when the OS window's pixel size changes.
    struct WindowResized
    {
        int width = 0;
        int height = 0;
    };

    /// @brief
    /// Purpose: Key transition for text/UI-style consumers; gameplay polls Input instead.
    struct KeyEvent
    {
        Key key = Key::UNKNOWN;
        bool is_down = false;
        bool is_repeat = false;
    };

    /// @brief
    /// Purpose: Fired on the main thread after a watched asset file changed and re-decoded.
    struct AssetReloaded
    {
        Uuid id = {};
    };

    /// @brief
    /// Purpose: Fired when two physics toys start touching (ToyId values; fed by the physics
    /// backend during the fixed step, delivered at the pump).
    struct CollisionEvent
    {
        uint32 toy_a = 0;
        uint32 toy_b = 0;
    };

    /// @brief
    /// Purpose: Fired after a script source recompiled; instances restart on their next update.
    struct ScriptReloaded
    {
        Uuid id = {};
    };

    /// @brief
    /// Purpose: The only events that exist, as named signals over one pump-drained queue.
    /// Later milestones add: asset_reloaded, collision, kit, script_reloaded.
    struct Events
    {
        EventQueue queue;
        Signal<KeyEvent> key {queue};
        Signal<WindowResized> window_resized {queue};
        Signal<AssetReloaded> asset_reloaded {queue};
        Signal<ScriptReloaded> script_reloaded {queue};
        Signal<CollisionEvent> collision {queue};

        /// @brief
        /// Purpose: Dispatches all queued events; called once per frame by Engine::pump().
        void drain()
        {
            queue.drain();
        }
    };
}
