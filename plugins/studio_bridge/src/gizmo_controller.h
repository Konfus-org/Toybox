#pragma once
#include "engine_services.h"
#include "gizmo_types.h"
#include "selection.h"
#include "view_manager.h"
#include "tbx/systems/graphics/camera_view.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/types/vectors.h"

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: Drives the editor's transform gizmo: tracks the active tool, hit-tests + drags its
    /// handles for the focused editor view each frame, and submits the editor overlay (selection
    /// highlight + transform handles) to the engine Gizmos service.
    /// @details
    /// Ownership: Owns the active gizmo mode; borrows the engine services (incl. the RPC host), view
    /// manager, and selection. Thread Safety: Main-thread only (driven from on_update).
    class GizmoController
    {
      public:
        GizmoController(EngineServices& services, ViewManager& views, Selection& selection);

        // Sets the active transform tool from a { mode } params object (notification from the toolbar).
        void set_mode(const tbx::Json& params);

        // Per-frame hover hit-test + drag for the focused editor view(s); writes the resulting
        // transform onto every selected entity and notifies the editor when a drag ends.
        void update(const tbx::DeltaTime& dt);

        // Submits the selection highlight (always) plus the active transform gizmo's handles for the
        // focused view (when one is given). World-space, so one submission draws in every view.
        void submit_overlay(const tbx::CameraView* focused_camera, const GizmoState* focused_gizmo);

      private:
        // World pivot for the gizmo: the average world position of the selected entities. False when
        // none.
        bool compute_pivot(tbx::World& world, tbx::Vec3& out_pivot) const;

        // Applies the in-progress drag (translate/rotate/scale) to every selected entity, deriving
        // the new transform from the drag anchor + current cursor ray.
        void apply_drag(
            tbx::World& world,
            GizmoState& gizmo,
            const tbx::CameraView& camera_view,
            const tbx::Vec3& ray_origin,
            const tbx::Vec3& ray_direction);

        EngineServices& _services;
        ViewManager& _views;
        Selection& _selection;
        // The active transform tool, set by the editor's toolbar (view.setGizmo).
        GizmoMode _gizmo_mode = GizmoMode::NONE;
    };
}
