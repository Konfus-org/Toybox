#pragma once
#include "draw_lane_state.h"
#include <memory>

namespace tbx::studio_bridge
{
    struct DataPlaneState;
    struct EngineServices;

    /// @brief Registers the draw lane's overlay render passes (tag-gated to editor cameras, the
    /// same scope filter as the gizmo-layer pass): the primitive pass and the sprite pass.
    /// @p textures is the editor-uploaded texture table sprite commands reference.
    void register_draw_lane_pass(
        DrawLaneState& lane,
        const EngineServices& services,
        const std::shared_ptr<EditorTextureTable>& textures);

    /// @brief Removes the pass before plugin unload so the renderer never calls freed code.
    void unregister_draw_lane_pass(DrawLaneState& lane, const EngineServices& services);

    /// @brief Decodes every draw-layer cell whose sequence moved since the last frame into its
    /// per-layer Gizmos batch (shape commands replay into the batch, raw vertex commands ride
    /// set_external), then republishes the scope list. An unchanged layer costs two atomic reads;
    /// its batch keeps rendering. Called once per frame from the bridge update.
    void submit_draw_lane(
        DrawLaneState& lane, DataPlaneState& plane, const EngineServices& services);
}
