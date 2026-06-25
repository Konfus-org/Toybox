#pragma once
#include "engine_services.h"
#include "selection.h"

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: Draws editor-only wireframe outlines for the SELECTED entities' physics collider/trigger
    /// shapes via the immediate-mode Gizmos service — green for solid colliders, and red (empty) turning
    /// green (occupied) for triggers.
    /// @details
    /// Ownership: Stateless; borrows the engine services and selection. Thread Safety: Main thread only;
    /// called once per frame from the bridge's gizmo-overlay submission. The geometry is world-space, so a
    /// single submission draws correctly in every editor view (game/asset-preview views skip the gizmo batch).
    class ColliderGizmos
    {
      public:
        ColliderGizmos(EngineServices& services, Selection& selection);

        // Appends the selected entities' collider/trigger wireframes to this frame's gizmo overlay.
        void submit();

      private:
        EngineServices& _services;
        Selection& _selection;
    };
}
