#pragma once
#include "tbx/interfaces/input_backend.h"

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: The editor-input mirror's state: the last mouse-lock mode pushed to the editor, so
    /// only changes are sent (input.mouseLock). Plain state: input_ops owns the behavior (editor fly
    /// + asset-preview orbit cameras, game input injection, and the mouse-lock report).
    /// @details
    /// Ownership: Owned by the plugin by value. Thread Safety: Main-thread only (driven from
    /// on_update).
    struct InputState
    {
        tbx::MouseLockMode last_reported_lock = tbx::MouseLockMode::UNLOCKED;
    };
}
