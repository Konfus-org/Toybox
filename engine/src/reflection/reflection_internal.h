#pragma once

namespace tbx::internal
{
    /// @brief
    /// Purpose: Registers every builtin type — blocks, asset types, and the App/.tapp schema —
    /// THE one registration call. Self-guards on is_reflection_ready(), so calling it twice is a
    /// no-op; run() (via initialize_assets) calls it during boot.
    void initialize_reflection();

    /// @brief
    /// Purpose: Drops every registered type so the next initialize_reflection() rebuilds from
    /// scratch — for tests that need a clean registry between cases. Not for runtime use.
    void purge_reflection_registry();
}
