#pragma once
#include "engine_services.h"
#include "gizmo_types.h"
#include "selection.h"
#include "view_manager.h"
#include "tbx/systems/graphics/camera_view.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/types/ray.h"
#include "tbx/types/uuid.h"
#include "tbx/types/vectors.h"
#include <functional>
#include <string>
#include <unordered_map>

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: Drives the editor's transform gizmo — its own thing: tracks the active tool, owns the
    /// per-view interaction state (hover/drag), hit-tests + drags the handles for the focused editor
    /// view each frame, and owns the gizmo overlay render pass that draws the handles.
    /// @details
    /// Ownership: Owns the active gizmo mode, the per-view gizmo states, and its registered overlay
    /// pass; holds non-owning references to the engine services (incl. the RPC host), the view manager,
    /// and selection. Thread Safety: Main-thread only (driven from on_update); the pass's execute runs
    /// on the render lane.
    class GizmoController
    {
      public:
        GizmoController(EngineServices& services, ViewManager& views, Selection& selection);

      public:
        /// @brief Registers the transform-gizmo overlay pass on the engine Rendering service (gated on
        /// the editor-camera tag); it draws the shared Gizmos batch submit_overlay fills each frame.
        void register_pass();

        /// @brief Removes the gizmo overlay pass before the plugin unloads.
        void unregister_pass();

        /// @brief Sets the active transform tool from a { mode } params object (toolbar notification).
        void set_mode(const tbx::Json& params);

        /// @brief Per-frame hover hit-test + drag for the focused editor view; writes the resulting
        /// transform onto every selected entity and notifies the editor when a drag ends.
        void update(const tbx::DeltaTime& dt);

        /// @brief Submits the active transform gizmo's handles for the focused (or first) editor view to
        /// the shared Gizmos batch. World-space, so one submission draws in every editor view.
        void submit_overlay();

      private:
        // World pivot for the gizmo: the average world position of the selected entities. False when
        // none.
        bool compute_pivot(tbx::World& world, tbx::Vec3& out_pivot) const;

        // Applies the in-progress drag (translate/rotate/scale) to every selected entity, deriving the
        // new transform from the drag anchor + current cursor ray.
        void apply_drag(
            tbx::World& world,
            GizmoState& gizmo,
            const tbx::CameraView& camera_view,
            const tbx::Ray& cursor,
            float cursor_u,
            float cursor_v);

        // Drops the gizmo interaction state of editor views that no longer exist.
        void prune_gizmo_states();

      private:
        std::reference_wrapper<EngineServices> _services;
        std::reference_wrapper<ViewManager> _views;
        std::reference_wrapper<Selection> _selection;
        // The active transform tool, set by the editor's toolbar (view.setGizmo).
        GizmoMode _gizmo_mode = GizmoMode::NONE;
        // Per-view interaction state (hover + in-progress drag), keyed by view name — owned here rather
        // than on the view stream so the gizmo is fully its own thing.
        std::unordered_map<std::string, GizmoState> _gizmo_states = {};
        // Id of the gizmo overlay pass on the engine Rendering service. Invalid until registered.
        tbx::Uuid _pass = {};
    };
}
