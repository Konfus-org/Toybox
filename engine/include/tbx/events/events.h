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

    /// @brief
    /// Purpose: The only events that exist, as named signals over one pump-drained queue.
    /// Later milestones add: asset_reloaded, collision, kit, script_reloaded.
    struct TBX_API Events
    {
        EventQueue queue;
        Signal<KeyEvent> key {queue};
        Signal<WindowResized> window_resized {queue};
        Signal<AssetReloaded> asset_reloaded {queue};
        Signal<AssetUnloaded> asset_unloaded {queue};
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
