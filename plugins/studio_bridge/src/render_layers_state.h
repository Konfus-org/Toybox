#pragma once
#include "tbx/systems/graphics/render_debug_view.h"

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: The editor's render-layers toggles (its Render Layers toolbar), pushed as one
    /// editor.setRenderLayers notification. The collider flags gate the collider-wireframe pass's
    /// per-frame submission; the post toggle + render stage are forwarded to the engine's rendering
    /// debug view (gated to editor cameras). Plain state: render_layers_ops owns the behavior.
    /// @details
    /// Ownership: Owned by the plugin by value. Thread Safety: Main-thread only (RPC dispatch and
    /// the per-frame submission both run there).
    struct RenderLayersState
    {
        // Draw every collider/trigger wireframe in the world, not just the selection's.
        bool colliders_all = false;

        // Draw the selected entities' collider/trigger wireframes (the long-standing default).
        bool colliders_selected = true;

        // Post-processing on editor views (the engine's debug view forces it off when false).
        bool post_processing = true;

        // The render-stage debug view editor views output (FINAL = the normal shaded frame).
        tbx::RenderDebugStage stage = tbx::RenderDebugStage::FINAL;
    };
}
