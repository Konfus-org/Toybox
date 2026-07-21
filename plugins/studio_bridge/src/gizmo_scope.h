#pragma once
#include "tbx/systems/graphics/gizmos.h"
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace tbx::studio_bridge
{
    /// @brief One rendered batch scope: the every-view geometry (empty view) or one view's. A pass
    /// draws a scope when its view is empty or tags the camera (editor cameras carry their view name).
    struct GizmoScope
    {
        std::string view = {};
        std::shared_ptr<tbx::Gizmos> gizmos = {};
    };

    /// @brief The scope list shared with a pass's execute callback, which can outlive an unregister
    /// by an in-flight frame — the callback keeps the table (and through it the batches) alive,
    /// never the plugin's state. Every read/write of `scopes` (main-thread rebuild, render-lane
    /// copy) holds `mutex`.
    struct GizmoScopeTable
    {
        std::mutex mutex = {};
        std::vector<GizmoScope> scopes = {};
    };
}
