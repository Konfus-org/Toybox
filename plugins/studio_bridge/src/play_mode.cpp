#include "play_mode.h"
#include "tbx/types/assets/world.h"
#include <utility>
#include <vector>

namespace tbx::studio_bridge
{
    PlayMode::PlayMode(EngineServices& services, SetPausedFn set_paused)
        : _services(services)
        , _set_paused(std::move(set_paused))
    {
    }

    void PlayMode::set_playing(bool playing)
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
        }

        _is_playing = playing;
    }

    void PlayMode::snapshot_world()
    {
        _runtime_snapshot.clear();
        _global_snapshot.clear();

        auto world = _services.active_world();
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

    void PlayMode::restore_world()
    {
        auto world = _services.active_world();
        if (world)
        {
            // Remove every entity play spawned or mutated, then replay the snapshot with each
            // entity's original persistence. View cameras are unaffected — they live in the view
            // manager's own registry.
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
}
