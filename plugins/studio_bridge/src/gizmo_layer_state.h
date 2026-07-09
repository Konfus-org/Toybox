#pragma once
#include "tbx/systems/files/json.h"
#include "tbx/systems/graphics/gizmos.h"
#include "tbx/types/uuid.h"
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace tbx::studio_bridge
{
    /// @brief One retained, named editor drawing layer (`gizmos.set` / `gizmos.remove`). A layer's
    /// drawing is a stream of compact ops — arrays whose head names the tbx::Gizmos method they
    /// replay into (["wire_box", [c], [s]]) — so the wire carries shapes, not vertices, and
    /// tessellation stays engine-side.
    struct GizmoLayer
    {
        tbx::Json ops = tbx::Json::array();
        bool is_visible = true;
        std::string view = {};  // The view name the layer is restricted to; empty = every editor view.
    };

    /// @brief One rendered batch scope: the every-view geometry (empty view) or one view's. The pass
    /// draws a scope when its view is empty or tags the camera (editor cameras carry their view name).
    struct GizmoScope
    {
        std::string view = {};
        std::shared_ptr<tbx::Gizmos> gizmos = {};
    };

    /// @brief The scope list shared with the layer pass's execute callback, which can outlive an
    /// unregister by an in-flight frame — the callback keeps the table (and through it the batches)
    /// alive, never the plugin's state. Every read/write of `scopes` (main-thread rebuild, render-lane
    /// copy) holds `mutex`.
    struct GizmoScopeTable
    {
        std::mutex mutex = {};
        std::vector<GizmoScope> scopes = {};
    };

    /// @brief
    /// Purpose: The engine half of the editor's gizmo-overlay API (Studio's Gizmos project): the
    /// retained layer table, the per-scope Gizmos batches (one batch for the every-view layers plus
    /// one per view-restricted view name), and the overlay pass id. Plain state: gizmo_layer_ops owns
    /// the behavior (pass lifecycle, gizmos.set/gizmos.remove, per-frame submit).
    /// @details
    /// Ownership: Owned by the plugin by value; `scopes` is shared with the registered pass's execute
    /// callback so an in-flight frame keeps the batches alive. Thread Safety: set/remove/submit run on
    /// the main thread; the render pass copies the scope list under GizmoScopeTable::mutex on the
    /// render lane (each Gizmos batch is itself mutex-guarded across the two). Batches rebuild only
    /// when a layer changed (`dirty`), so a static overlay costs nothing per frame.
    struct GizmoLayerState
    {
        std::map<std::string, GizmoLayer> layers = {};
        // Batches persist across rebuilds keyed by view scope, so their lazily-built pipelines survive.
        std::map<std::string, std::shared_ptr<tbx::Gizmos>> batches = {};
        bool dirty = false;
        std::shared_ptr<GizmoScopeTable> scopes = {};
        tbx::Uuid pass = {};
    };
}
