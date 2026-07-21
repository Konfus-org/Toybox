#pragma once
#include "tbx/systems/files/json.h"
#include "tbx/utils/result.h"
#include <string>

namespace tbx::studio_bridge
{
    struct EngineServices;
    struct GlassState;

    /// @brief Replaces one view's frosted-glass rects from a view.setGlass payload
    /// ({ view, rects: [[x, y, width, height], ...] (normalized, top-left origin), radius? }): the
    /// view's blur pass is rebuilt to cover the new rects (up to seven — the shader's parameter-lane
    /// budget), removed entirely when the list is empty, and left untouched when nothing changed.
    Result set_view_glass(GlassState& state, const EngineServices& services, const tbx::Json& params);

    /// @brief Removes one view's glass pass (the view stopped).
    void clear_view_glass(GlassState& state, const EngineServices& services, const std::string& view);

    /// @brief Removes every view's glass pass (editor disconnect / plugin teardown — the passes'
    /// vtables live in this plugin).
    void clear_all_glass(GlassState& state, const EngineServices& services);
}
