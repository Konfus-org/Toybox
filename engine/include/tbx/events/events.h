#pragma once
#include "tbx/utils/api.h"
#include "tbx/utils/typedefs.h"
#include "tbx/utils/uuid.h"
#include "tbx/events/event_queue.h"
#include "tbx/events/signal.h"
#include "tbx/platform/keys.h"

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
    /// Purpose: Fired on the main thread after a watched asset file changed and re-decoded.
    struct TBX_API AssetReloaded
    {
        Uuid id = {};
        // The asset file's extension (".luau", ".png", ...) so listeners filter without a
        // lookup; events must stay trivially copyable, hence the fixed buffer.
        char extension[16] = {};
    };

    /// @brief
    /// Purpose: Fired on the main thread when the asset system unloads an idle asset —
    /// caches keyed on the asset (GPU uploads, documents) drop their copies on this.
    struct TBX_API AssetUnloaded
    {
        Uuid id = {};
        char extension[16] = {};
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

}

// The only events that exist, as named signals over one pump-drained queue. Module state is
// created on first use; reset() drops every subscription and queued event.
namespace tbx::events
{
    TBX_API Signal<AssetReloaded>& asset_reloaded();
    TBX_API Signal<AssetUnloaded>& asset_unloaded();
    TBX_API Signal<CollisionEvent>& collision();
    TBX_API Signal<KeyEvent>& key();
    TBX_API Signal<ScriptReloaded>& script_reloaded();
    TBX_API Signal<WindowResized>& window_resized();

    /// @brief
    /// Purpose: Dispatches all queued events; called once per frame by the runtime's pump.
    TBX_API void drain();

    /// @brief
    /// Purpose: Drops every subscription and queued event; the next call starts fresh. run()
    /// calls this at shutdown.
    TBX_API void reset();
}
