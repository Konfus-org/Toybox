#pragma once
#include "engine_services.h"
#include "selection.h"
#include "tbx/systems/graphics/gizmos.h"
#include "tbx/types/uuid.h"
#include <functional>
#include <memory>

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: A registered editor render pass that draws wireframe outlines for the SELECTED
    /// entities' physics collider/trigger shapes — green for solid colliders, and red (empty) turning
    /// green (occupied) for triggers. It owns its own immediate-mode Gizmos batch and an Overlay
    /// RenderPass gated on the editor-camera tag, so the wireframes draw only in editor viewports and
    /// independently of the transform-gizmo overlay.
    /// @details
    /// Ownership: Owns its dedicated Gizmos instance and its registered pass id; holds non-owning
    /// references to the engine services and selection. Thread Safety: submit() (main thread) populates
    /// the batch each frame; the pass renders it on the render lane (the Gizmos batch is mutex-guarded
    /// across the two). The geometry is world-space, so one batch draws correctly in every editor view.
    class ColliderPass
    {
      public:
        ColliderPass(EngineServices& services, Selection& selection);

      public:
        /// @brief Registers the collider overlay pass on the engine's Rendering service (and lazily
        /// builds its dedicated Gizmos batch). Call once the rendering + backend services are resolved.
        void register_pass();

        /// @brief Removes the collider overlay pass before the plugin unloads.
        void unregister_pass();

        /// @brief Rebuilds this frame's collider/trigger wireframe batch from the current selection. Run
        /// once per frame on the main thread before the views render.
        void submit();

      private:
        std::reference_wrapper<EngineServices> _services;
        std::reference_wrapper<Selection> _selection;

        // The bridge's own gizmo batch, separate from the shared service so the collider wireframes are
        // an independent pass rather than sharing the transform-gizmo overlay's batch. Null until
        // register_pass builds it.
        std::shared_ptr<tbx::Gizmos> _gizmos = {};

        // Id of the collider overlay pass on the Rendering service, kept so it can be removed before
        // the plugin unloads. Invalid until registered.
        tbx::Uuid _pass = {};
    };
}
