#include "game_mode_ops.h"
#include "engine_services.h"
#include "game_mode_state.h"
#include "physics_event_ops.h"
#include "sync_event_state.h"
#include "tbx/types/assets/world.h"
#include <vector>

namespace tbx::studio_bridge
{
    static void snapshot_world(GameModeState& game_mode, const EngineServices& services)
    {
        game_mode.runtime_snapshot.clear();
        game_mode.global_snapshot.clear();

        auto world = services.active_world();
        if (!world)
            return;

        for (const auto& entity : world->get_all())
        {
            // Preserve each entity's persistence so restore re-adds it as runtime or global as it
            // was.
            if (world->is_global(entity.get_id()))
                game_mode.global_snapshot.absorb(entity);
            else
                game_mode.runtime_snapshot.absorb(entity);
        }
    }

    static void restore_world(GameModeState& game_mode, const EngineServices& services)
    {
        auto world = services.active_world();
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

            world->add_entities(game_mode.runtime_snapshot);
            if (!game_mode.global_snapshot.is_empty())
            {
                auto globals = tbx::WorldGlobals();
                globals.entities = game_mode.global_snapshot;
                world->load_globals(globals);
            }
        }

        game_mode.runtime_snapshot.clear();
        game_mode.global_snapshot.clear();
    }

    static void reset_simulation(const EngineServices& services)
    {
        // Drop every physics body so the next play rebuilds them from the restored transforms (and
        // discard the in-flight step, whose result is from the played world).
        if (auto physics = services.physics.lock())
            physics->reset();

        // Destroy every live script instance so the next play re-runs on_start from the restored
        // bindings rather than resuming the previous session's per-script state.
        if (auto scripts = services.script_system.lock())
            scripts->reset();
    }

    void set_playing(
        GameModeState& game_mode,
        SyncEventState& events,
        const EngineServices& services,
        const std::function<void(bool paused)>& set_paused,
        bool playing)
    {
        if (game_mode.is_playing == playing)
            return;

        // Entering play snapshots the world before it mutates, then unpauses the engine to start
        // simulating. Exiting pauses first, then restores the snapshot, so a play session leaves no
        // trace. The engine itself only knows about the neutral pause; play-mode is ours.
        if (playing)
        {
            snapshot_world(game_mode, services);
            set_paused(false);
            // The live entities are the ones that will simulate this session; attach the editor's
            // physics-event forwarders now (after the snapshot, so the clean snapshot is restored
            // callback-free on exit). A mid-play subscription binds itself the same way.
            bind_subscribed_physics_events(events, services);
        }
        else
        {
            set_paused(true);
            restore_world(game_mode, services);
            // The world data is back to its pre-play snapshot, but the simulation systems still hold
            // state built from the played world (physics bodies/velocities, an in-flight step, live
            // script instances). Reset them too so leaving play is a full reset, not just a data swap.
            reset_simulation(services);
            // restore_world rebuilt every entity, dropping the runtime callback lists (and thus our
            // forwarders); just forget the bound set so the next play re-binds cleanly.
            events.bound_entities.clear();
        }

        game_mode.is_playing = playing;
    }
}
