#pragma once
#include "engine_services.h"
#include "tbx/systems/ecs/registry.h"
#include <functional>

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: Owns play-mode, which lives in the bridge rather than the engine: snapshots the whole
    /// game world on entering play and restores it on exit, driving the engine's neutral pause to gate
    /// simulation.
    /// @details
    /// Ownership: Owns the runtime/global world snapshots and the is-playing flag. Borrows the engine
    /// services and a pause callback. Thread Safety: Main-thread only.
    class PlayMode
    {
      public:
        // Pauses/unpauses the engine simulation (wired to the application's pause request).
        using SetPausedFn = std::function<void(bool paused)>;

        PlayMode(EngineServices& services, SetPausedFn set_paused);

        // Enters/exits play: snapshot then unpause on enter; pause then restore on exit. No-op when
        // already in the requested state.
        void set_playing(bool playing);
        bool is_playing() const { return _is_playing; }

      private:
        void snapshot_world();
        void restore_world();
        // Resets the simulation systems (physics, scripts) so no state held outside the ECS — body
        // positions/velocities, in-flight steps, per-script runtime state — survives the world restore.
        void reset_simulation();

        EngineServices& _services;
        SetPausedFn _set_paused;
        bool _is_playing = false;

        // Deep copy of the game world captured when play begins; replayed to restore on stop. Split by
        // persistence so each entity is restored as runtime or global exactly as it was.
        tbx::EntityRegistry _runtime_snapshot = {};
        tbx::EntityRegistry _global_snapshot = {};
    };
}
