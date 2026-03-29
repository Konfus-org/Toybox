#pragma once

namespace jolt_physics
{
    /// @brief
    /// Purpose: Centralizes allocator, factory, type registration, and diagnostic callback setup.
    /// @details
    /// Ownership: Owns process-wide runtime reference counting only; does not own plugin state.
    /// Thread Safety: Thread-safe; acquire/release are synchronized internally.
    class JoltRuntimeLifetime final
    {
      public:
        static bool acquire();
        static void release();
    };
}
