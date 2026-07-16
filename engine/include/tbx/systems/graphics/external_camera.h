#pragma once
#include "tbx/systems/graphics/camera_view.h"
#include "tbx/tbx_api.h"
#include "tbx/types/assets/world.h"
#include "tbx/types/render_target.h"
#include "tbx/types/uuid.h"
#include <memory>

namespace tbx
{
    /// @brief
    /// Purpose: A camera the engine renders that is NOT an entity in the active game world — an editor
    /// viewport or asset-preview camera supplied by a host (Studio). It carries the pose + lens to
    /// render from (`view`), the target to render into (`target`, usually a RenderTexture the host
    /// samples), and the world to render (`world_override`; the active world when null). The owner
    /// updates these between frames; the engine snapshots them by value once per frame for the render
    /// lane.
    /// @details
    /// Ownership: Value bundle — copies the CameraView/RenderTarget and shares the override world by
    /// shared_ptr. Thread Safety: Registered/updated/removed from the main thread; the render lane only
    /// ever sees the per-frame snapshot the engine takes for it.
    struct TBX_API ExternalCamera
    {
        CameraView view = {};
        RenderTarget target = {};
        std::shared_ptr<World> world_override = {};
        // When true, the engine renders this camera every frame. When false, it renders at a reduced
        // idle rate (see render_external_cameras) — the owner (the editor) sets it false for a viewport
        // that is idle (not focused, not being edited, not playing) so an untouched pane stops paying a
        // full render every frame. Never freezes: an idle camera still refreshes periodically. Defaults
        // true so a non-editor owner (or one that never sets it) renders every frame as before.
        bool render_active = true;
    };

    /// @brief The id register_external_camera returns; identifies a registered external camera for
    /// later update / unregister.
    using ExternalCameraId = Uuid;
}
