#pragma once
#include "tbx/systems/files/json.h"

namespace tbx::studio_bridge
{
    struct EngineServices;
    struct RenderLayersState;

    /// @brief Applies an editor.setRenderLayers payload: updates the collider wireframe flags the
    /// per-frame submission reads, and pushes the post toggle + render stage to the engine's
    /// rendering debug view, gated to editor cameras. Missing params keep their current values.
    void set_render_layers(
        RenderLayersState& layers, const EngineServices& services, const tbx::Json& params);
}
