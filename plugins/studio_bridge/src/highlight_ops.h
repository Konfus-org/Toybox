#pragma once

namespace tbx::studio_bridge
{
    struct EngineServices;
    struct HighlightState;

    /// @brief Registers the editor highlight render pass: a Post pass gated on the editor.camera tag
    /// that carries the selection-outline post effect, itself gated on the editor.selected entity tag
    /// (the bridge stamps it via selection.set). Purely per-tag — the engine renderer stays unaware
    /// of selection; game and asset-preview views never outline. Call once when the rendering
    /// service resolves.
    void register_editor_highlights(HighlightState& state, const EngineServices& services);

    /// @brief Removes the highlight pass (plugin teardown) so the renderer never runs a pass whose
    /// vtable lives in this plugin after it unloads.
    void unregister_editor_highlights(HighlightState& state, const EngineServices& services);
}
