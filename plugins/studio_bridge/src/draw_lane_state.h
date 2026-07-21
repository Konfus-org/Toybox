#pragma once
#include "data_plane_layout.h"
#include "gizmo_scope.h"
#include "texture_state.h"
#include "tbx/types/color.h"
#include "tbx/types/typedefs.h"
#include "tbx/types/uuid.h"
#include "tbx/types/vectors.h"
#include <array>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

namespace tbx::studio_bridge
{
    /// @brief One decoded sprite command: a camera-facing textured quad (the editor's icon path).
    struct SpriteInstance
    {
        uint64 texture = 0U;
        tbx::Vec3 center = {};
        tbx::Vec2 size = {};
        tbx::Color tint = {1.0F, 1.0F, 1.0F, 1.0F};
    };

    /// @brief One view scope's sprites, mirroring GizmoScope: drawn when the scope's view is empty
    /// or tags the camera.
    struct SpriteScope
    {
        std::string view = {};
        std::vector<SpriteInstance> sprites = {};
    };

    /// @brief The sprite scopes shared with the sprite pass's execute callback (same lifetime rules
    /// as GizmoScopeTable).
    struct SpriteScopeTable
    {
        std::mutex mutex = {};
        std::vector<SpriteScope> scopes = {};
    };

    /// @brief The sprite pass's lazily-built GPU pipeline, shared with its execute callback (render
    /// lane) which builds and owns it.
    struct SpritePassGpu
    {
        tbx::GpuId pipeline = tbx::INVALID_GPU_ID;
        bool failed = false;
    };

    /// @brief
    /// Purpose: The engine half of the editor's draw lane — the decoded per-layer tbx::Gizmos
    /// batches, the drain bookkeeping (a layer whose cell sequence hasn't moved costs nothing per
    /// frame), and the tag-gated overlay pass that renders them. Reuses the gizmo-layer scope table
    /// (GizmoScopeTable / GizmoScope): the pass's execute is the identical view-scope filter. Plain
    /// state: draw_lane_ops owns the behavior (pass lifecycle + the per-frame decode).
    /// @details
    /// Ownership: Owned by the plugin by value; `scopes` is shared with the registered pass's
    /// execute callback so an in-flight frame keeps the batches alive. Thread Safety: decode runs on
    /// the main thread; the pass copies the scope list under the table mutex on the render lane
    /// (each Gizmos batch is itself mutex-guarded across the two).
    struct DrawLaneState
    {
        // The last consumed seqlock sequence per layer cell (0 = never decoded).
        std::array<uint32, DATA_PLANE_DRAW_LAYER_COUNT> last_sequence = {};
        // One persistent batch per layer, so its lazily-built pipelines survive redecodes.
        std::array<std::shared_ptr<tbx::Gizmos>, DATA_PLANE_DRAW_LAYER_COUNT> batches = {};
        // Each layer's resolved view scope (empty = every editor view), refreshed at decode.
        std::array<std::string, DATA_PLANE_DRAW_LAYER_COUNT> scope_views = {};
        // Layers whose payload overflowed / was malformed, warned once each.
        std::array<bool, DATA_PLANE_DRAW_LAYER_COUNT> warned = {};
        std::shared_ptr<GizmoScopeTable> scopes = {};
        tbx::Uuid pass = {};
        // The seqlock copy target, reused across frames (grown once to the largest layer seen).
        std::vector<uint8> scratch = {};

        // The sprite half: per-layer decoded instances, the scope table + pipeline shared with the
        // sprite pass, and the editor-uploaded textures its commands reference.
        std::array<std::vector<SpriteInstance>, DATA_PLANE_DRAW_LAYER_COUNT> sprites = {};
        std::shared_ptr<SpriteScopeTable> sprite_scopes = {};
        std::shared_ptr<SpritePassGpu> sprite_gpu = {};
        std::shared_ptr<EditorTextureTable> textures = {};
        tbx::Uuid sprite_pass = {};

        // Mesh handles already warned about (unresolvable/empty), so a bad handle logs once.
        std::unordered_set<uint64> warned_meshes = {};
    };
}
