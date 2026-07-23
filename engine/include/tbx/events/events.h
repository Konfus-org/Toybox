#pragma once
#include "tbx/api.h"
#include "tbx/events/queue.h"
#include "tbx/events/signal.h"
#include "tbx/platform/keys.h"
#include "tbx/utils/typedefs.h"
#include "tbx/utils/uuid.h"
#include <array>
#include <functional>
#include <memory>
#include <typeinfo>
#include <unordered_map>

// App-wide events over one pump-drained queue, keyed by event TYPE — raise_event<E>()/on_event<E>()
// reach the same Signal<E> without a hand-maintained member list. The state is runtime.events;
// update_events dispatches everything queued once per frame.
namespace tbx
{
    /// @brief
    /// Purpose: Fired when the OS window's pixel size changes.
    struct TBX_DLL_EXPORT WindowResized
    {
        int width = 0;
        int height = 0;
    };

    /// @brief
    /// Purpose: An input transition for text/UI-style consumers; gameplay polls Input instead.
    /// Currently carries keyboard transitions; the generic name leaves room for other sources.
    struct TBX_DLL_EXPORT InputEvent
    {
        Key key = Key::UNKNOWN;
        bool is_down = false;
        bool is_repeat = false;
    };

    /// @brief
    /// Purpose: Fired when a controller is plugged in and the engine has claimed a slot for it;
    /// index is that slot [0, MAX_GAMEPADS). Anything wanting per-device setup (UI prompts,
    /// player assignment) listens here rather than polling connection state.
    struct TBX_DLL_EXPORT InputDeviceConnected
    {
        int index = 0;
    };

    /// @brief
    /// Purpose: Fired when a controller is unplugged; index is the slot it vacated. The engine
    /// has already released the device and cleared the slot's state by the time this dispatches.
    struct TBX_DLL_EXPORT InputDeviceDisconnected
    {
        int index = 0;
    };

    /// @brief
    /// Purpose: Fired on the main thread after an asset is first decoded and made available.
    struct TBX_DLL_EXPORT AssetLoaded
    {
        Uuid id = {};
        // The asset file's extension (".luau", ".png", ...) so listeners filter without a lookup.
        std::array<char, 16> extension = {};
    };

    /// @brief
    /// Purpose: Fired on the main thread after a watched asset file changed and re-decoded.
    struct TBX_DLL_EXPORT AssetReloaded
    {
        Uuid id = {};
        // The asset file's extension (".luau", ".png", ...) so listeners filter without a
        // lookup; events must stay trivially copyable, hence the fixed buffer (zero-filled,
        // so .data() is always a terminated C string).
        std::array<char, 16> extension = {};
    };

    /// @brief
    /// Purpose: Fired on the main thread when the asset system unloads an idle asset —
    /// caches keyed on the asset (GPU uploads, documents) drop their copies on this.
    struct TBX_DLL_EXPORT AssetUnloaded
    {
        Uuid id = {};
        std::array<char, 16> extension = {};
    };

    /// @brief
    /// Purpose: Fired when two physics toys start touching (ToyId values; fed by the physics
    /// backend during the fixed step, delivered at the pump).
    struct TBX_DLL_EXPORT CollisionEvent
    {
        uint32 toy_a = 0;
        uint32 toy_b = 0;
    };

    /// @brief
    /// Purpose: The events module's state, held by value on the Runtime. One queue plus a type-keyed
    /// table of signals: signal<E>() lazily creates Signal<E> the first time an event type is raised or
    /// subscribed, so any (engine or script-defined) event type just works — no member to add.
    struct TBX_DLL_EXPORT EventsState
    {
        Queue queue;
        std::unordered_map<size, std::unique_ptr<ISignal>> signals;

        /// @brief
        /// Purpose: The one signal for event type E, created on first use (queued/deferred delivery).
        template <typename TEvent>
        Signal<TEvent>& signal()
        {
            std::unique_ptr<ISignal>& slot = signals[typeid(TEvent).hash_code()];
            if (!slot)
                slot = std::make_unique<Signal<TEvent>>(queue);
            return *static_cast<Signal<TEvent>*>(slot.get());
        }
    };

}
