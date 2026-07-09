#pragma once
#include "tbx/systems/files/json.h"
#include "tbx/utils/result.h"

namespace tbx::studio_bridge
{
    struct EngineServices;
    struct GizmoLayerState;

    /// @brief Registers the layer overlay pass on the engine's Rendering service. Call once the
    /// rendering + backend services are resolved.
    void register_gizmo_layer_pass(GizmoLayerState& layers, const EngineServices& services);

    /// @brief Removes the layer overlay pass before the plugin unloads.
    void unregister_gizmo_layer_pass(GizmoLayerState& layers, const EngineServices& services);

    /// @brief Rebuilds the per-scope batches if a layer changed since the last frame. Run once per
    /// frame on the main thread before the views render.
    void submit_gizmo_layers(GizmoLayerState& layers, const EngineServices& services);

    /// @brief gizmos.set: writes one layer value ({address: "gizmos/<name>", key, value}); the
    /// addressed layer is created on its first write.
    tbx::Result set_gizmo_layer(GizmoLayerState& layers, const tbx::Json& params);

    /// @brief gizmos.remove: drops the addressed layer on this side.
    tbx::Result remove_gizmo_layer(GizmoLayerState& layers, const tbx::Json& params);
}
