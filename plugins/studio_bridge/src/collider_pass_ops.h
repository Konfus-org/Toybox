#pragma once

namespace tbx::studio_bridge
{
    struct ColliderPassState;
    struct EngineServices;
    struct RenderLayersState;
    struct SelectionState;

    /// @brief Registers the collider-wireframe overlay pass on the engine's Rendering service (and
    /// lazily builds its dedicated Gizmos batch): green for solid colliders, red (empty) turning
    /// green (occupied) for triggers, gated on the editor-camera tag so it draws only in editor
    /// viewports. Call once the rendering + backend services are resolved.
    void register_collider_pass(ColliderPassState& pass, const EngineServices& services);

    /// @brief Removes the collider overlay pass before the plugin unloads.
    void unregister_collider_pass(ColliderPassState& pass, const EngineServices& services);

    /// @brief Rebuilds this frame's collider/trigger wireframe batch: every collider in the world
    /// when the render layers ask for all, the current selection's when they ask for
    /// selection-only (both when both). Run once per frame on the main thread before the views
    /// render.
    void submit_collider_wireframes(
        ColliderPassState& pass,
        const SelectionState& selection,
        const RenderLayersState& layers,
        const EngineServices& services);
}
