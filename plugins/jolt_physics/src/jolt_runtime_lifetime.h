#pragma once

namespace jolt_physics
{
    /// <summary>Manages shared Jolt runtime initialization lifetime across plugin instances.</summary>
    /// <remarks>
    /// Purpose: Centralizes allocator, factory, type registration, and diagnostic callback setup.
    /// Ownership: Owns process-wide runtime reference counting only; does not own plugin state.
    /// Thread Safety: Thread-safe; acquire/release are synchronized internally.
    /// </remarks>
    class JoltRuntimeLifetime final
    {
      public:
        static bool acquire();
        static void release();
    };
}
