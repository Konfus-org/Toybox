#pragma once
#include "tbx/events/event_queue.h"
#include "tbx/events/signal.h"
#include "tbx/platform/keys.h"
#include "tbx/utils/api.h"
#include "tbx/utils/typedefs.h"
#include "tbx/utils/uuid.h"
#include <array>

// The only events that exist, as named signals over one pump-drained queue — the state is
// runtime.events: emit and subscribe on its members directly (runtime.events.key.emit(...))
// and events::update dispatches everything queued once per frame.
namespace tbx::events
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
        EventQueue queue;
        Signal<KeyEvent> key {queue};
        Signal<WindowResized> window_resized {queue};
        Signal<AssetReloaded> asset_reloaded {queue};
        Signal<AssetUnloaded> asset_unloaded {queue};
        Signal<ScriptReloaded> script_reloaded {queue};
        Signal<CollisionEvent> collision {queue};
    };

    /// @brief
    /// Purpose: Dispatches everything queued since the last update, in emission order — the
    /// events module's per-frame verb; tbx::run() calls it during the pump. Events emitted
    /// during an update land in the next one.
    TBX_API void update(EventsState& state);
}
