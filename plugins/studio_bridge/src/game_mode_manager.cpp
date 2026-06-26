#include "game_mode_manager.h"
#include "tbx/types/assets/world.h"
#include <utility>
#include <vector>

namespace tbx::studio_bridge
{
    GameModeManager::GameModeManager(EngineServices& services, SetPausedFn set_paused)
        : _services(services)
        , _set_paused(std::move(set_paused))
    {
    }

    void GameModeManager::set_playing(bool playing)
    {
        if (_is_playing == playing)
            return;

        // Entering play snapshots the world before it mutates, then unpauses the engine to start
        // simulating. Exiting pauses first, then restores the snapshot, so a play session leaves no
        // trace. The engine itself only knows about the neutral pause; play-mode is ours.
        if (playing)
        {
            snapshot_world();
            _set_paused(false);
        }
        else
        {
            _set_paused(true);
            restore_world();
            // The world data is back to its pre-play snapshot, but the simulation systems still hold
            // state built from the played world (physics bodies/velocities, an in-flight step, live
            // script instances). Reset them too so leaving play is a full reset, not just a data swap.
            reset_simulation();
        }

        _is_playing = playing;
    }

    void GameModeManager::snapshot_world()
    {
        _runtime_snapshot.clear();
        _global_snapshot.clear();

        auto world = _services.get().active_world();
        if (!world)
            return;

        for (const auto& entity : world->get_all())
        {
            // Preserve each entity's persistence so restore re-adds it as runtime or global as it
            // was.
            if (world->is_global(entity.get_id()))
                _global_snapshot.absorb(entity);
            else
                _runtime_snapshot.absorb(entity);
        }
    }

    void GameModeManager::restore_world()
    {
        auto world = _services.get().active_world();
        if (world)
        {
            // Remove every entity play spawned or mutated, then replay the snapshot with each
            // entity's original persistence. View cameras are unaffected — they live in the bridge's
            // own rendering registry.
            auto to_destroy = std::vector<tbx::Uuid>();
            for (const auto& entity : world->get_all())
                to_destroy.push_back(entity.get_id());

            for (const auto& id : to_destroy)
            {
                auto entity = world->get(id);
                if (entity.get_id().is_valid())
                    world->destroy(entity);
            }

            world->add_entities(_runtime_snapshot);
            if (!_global_snapshot.is_empty())
            {
                auto globals = tbx::WorldGlobals();
                globals.entities = _global_snapshot;
                world->load_globals(globals);
            }
        }

        _runtime_snapshot.clear();
        _global_snapshot.clear();
    }

    void GameModeManager::reset_simulation()
    {
        // Drop every physics body so the next play rebuilds them from the restored transforms (and
        // discard the in-flight step, whose result is from the played world).
        if (auto physics = _services.get().physics.lock())
            physics->reset();

        // Destroy every live script instance so the next play re-runs on_start from the restored
        // bindings rather than resuming the previous session's per-script state.
        if (auto scripts = _services.get().script_system.lock())
            scripts->reset();
    }
}
