#pragma once
#include "tbx/ui/ui.h"

namespace tbx::internal
{
    /// @brief
    /// Purpose: Advances animations/layout, evaluates bindings, and retires long-undrawn
    /// documents. Called by tbx::run() every frame.
    void update_ui(UiState& state, float delta_time);
}
