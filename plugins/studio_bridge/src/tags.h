#pragma once
#include <string>

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: The runtime gameplay tags the bridge stamps on entities and view cameras so the
    /// engine's tag-gated render passes (selection outline + gizmo overlay) apply only where the
    /// editor wants them. Runtime tags never serialize, so none of this leaks into a saved world.
    namespace Tags
    {
        // Stamped on selected entities; the editor's selection-outline post effect is gated on it.
        inline const std::string SELECTED = "editor.selected";

        // Stamped on the bridge's editor view cameras (which live in the bridge's own registry, never
        // the game world); the editor render passes are gated on it, so game and asset-preview views
        // never receive them.
        inline const std::string EDITOR_CAMERA = "editor.camera";
    }
}
