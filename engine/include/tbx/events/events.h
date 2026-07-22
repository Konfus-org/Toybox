#pragma once
#include "tbx/api.h"
#include "tbx/events/queue.h"
#include "tbx/events/signal.h"
#include "tbx/platform/keys.h"
#include "tbx/utils/typedefs.h"
#include "tbx/utils/uuid.h"
#include <array>

// The only events that exist, as named signals over one pump-drained queue — the state is
// runtime.events: emit and subscribe on its members directly (runtime.events.key.emit(...))
// and update_events dispatches everything queued once per frame.
namespace tbx
{
    /// @brief
    /// Purpose: Fired when the OS window's pixel size changes.
    struct TBX_API WindowResized
    {
        int width = 0;
        int height = 0;
    };

    /// @brief
    /// Purpose: Key transition for text/UI-style consumers; gameplay polls Input instead.
    struct TBX_API KeyEvent
    {
        Key key = Key::UNKNOWN;
        bool is_down = false;
        bool is_repeat = false;
    };

    /// @brief
    /// Purpose: Fired when a controller is plugged in and the engine has claimed a slot for it;
    /// index is that slot [0, MAX_GAMEPADS). Anything wanting per-device setup (UI prompts,
    /// player assignment) listens here rather than polling connection state.
    struct TBX_API InputDeviceConnected
    {
        int index = 0;
    };

    /// @brief
    /// Purpose: Fired when a controller is unplugged; index is the slot it vacated. The engine
    /// has already released the device and cleared the slot's state by the time this dispatches.
    struct TBX_API InputDeviceDisconnected
    {
        int index = 0;
    };

    /// @brief
    /// Purpose: Fired on the main thread after a watched asset file changed and re-decoded.
    struct TBX_API AssetReloaded
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
    struct TBX_API AssetUnloaded
    {
        Uuid id = {};
        std::array<char, 16> extension = {};
    };

    /// @brief
    /// Purpose: Fired when two physics toys start touching (ToyId values; fed by the physics
    /// backend during the fixed step, delivered at the pump).
    struct TBX_API CollisionEvent
    {
        uint32 toy_a = 0;
        uint32 toy_b = 0;
    };

    /// @brief
    /// Purpose: Fired after a script source recompiled; instances restart on their next update.
    struct TBX_API ScriptReloaded
    {
        Uuid id = {};
    };

    /// @brief
    /// Purpose: The events module's state, held by value on the Runtime; adding an event
    /// means adding a member, deliberately.
    struct TBX_API EventsState
    {
        Queue queue;
        Signal<KeyEvent> key {queue};
        Signal<WindowResized> window_resized {queue};
        Signal<AssetReloaded> asset_reloaded {queue};
        Signal<AssetUnloaded> asset_unloaded {queue};
        Signal<ScriptReloaded> script_reloaded {queue};
        Signal<CollisionEvent> collision {queue};
        Signal<InputDeviceConnected> input_device_connected {queue};
        Signal<InputDeviceDisconnected> input_device_disconnected {queue};
    };

    /// @brief
    /// Purpose: Drops every subscription registered under the given owner tag from every signal
    /// at once — the bulk teardown a language backend runs before it tears down (so no handler
    /// capturing a dying VM survives to be dispatched). Extend this when adding a signal.
    TBX_API void unsubscribe_all(EventsState& state, const void* owner);

    /// @brief
    /// Purpose: Dispatches everything queued since the last update, in emission order — the
    /// events module's per-frame verb; tbx::run() calls it during the pump. Events emitted
    /// during an update land in the next one.
    TBX_API void update_events(EventsState& state);
}
