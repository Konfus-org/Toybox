#include "tbx/systems/ecs/world/manager.h"
#include "chunk_loader.h"
#include "streamer.h"
#include "tbx/systems/debugging/macros.h"
#include <algorithm>
#include <utility>
#include <vector>

namespace tbx
{
    //// INTERNAL ////

    struct WorldManager::State
    {
        State(std::weak_ptr<AssetManager> asset_manager)
            : asset_manager(std::move(asset_manager))
            , chunk_loader(std::make_unique<ChunkLoader>(this->asset_manager))
            , entity_streamer(std::make_unique<EntityStreamer>())
        {
        }

        std::weak_ptr<AssetManager> asset_manager = {};
        std::unique_ptr<ChunkLoader> chunk_loader = {};
        std::unique_ptr<EntityStreamer> entity_streamer = {};
        std::shared_ptr<World> active_world = {};
        Handle active_world_handle = {};
        std::vector<Handle> loaded_globals = {};
        bool active_world_from_asset = false;
    };

    //// WORLD MANAGER IMPL ////

    WorldManager::WorldManager(std::weak_ptr<AssetManager> asset_manager)
        : _state(std::make_unique<State>(std::move(asset_manager)))
    {
    }

    WorldManager::~WorldManager() noexcept
    {
        TBX_TRY_CATCH_ASSERT(clear_active_world();, "Toybox world manager shutdown failed.");
    }

    void WorldManager::clear_active_world()
    {
        release_active_world();
        _state->active_world = {};
        _state->active_world_handle = {};
        _state->active_world_from_asset = false;
    }

    Handle WorldManager::get_active_world_handle() const
    {
        return _state->active_world_handle;
    }

    std::weak_ptr<World> WorldManager::get_active_world() const
    {
        return _state->active_world;
    }

    bool WorldManager::has_active_world() const
    {
        return _state->active_world != nullptr;
    }

    bool WorldManager::set_active_world(const Handle& handle)
    {
        if (!handle.is_valid())
            return false;

        if (_state->active_world && _state->active_world_from_asset
            && _state->active_world_handle == handle)
            return true;

        const auto asset_manager = _state->asset_manager.lock();
        if (!asset_manager)
            return false;

        auto loaded_world = asset_manager->load<World>(handle);
        if (!loaded_world)
            return false;

        if (!load_world_globals(*loaded_world))
            return false;
        if (!_state->chunk_loader->set_chunks(*loaded_world))
            return false;

        release_active_world();
        asset_manager->set_pinned(handle, true);
        _state->active_world = std::move(loaded_world);
        _state->active_world_handle = handle;
        _state->active_world_from_asset = true;
        return true;
    }

    bool WorldManager::set_active_world(std::shared_ptr<World> world)
    {
        if (!world)
            return false;

        if (!_state->chunk_loader->set_chunks(*world))
            return false;

        release_active_world();
        _state->active_world_handle = Handle(world->id);
        _state->active_world = std::move(world);
        _state->active_world_from_asset = false;
        return true;
    }

    void WorldManager::update(const DeltaTime& dt, const WorldSettings& settings)
    {
        if (!_state->active_world)
            return;

        _state->chunk_loader->update(
            *_state->active_world,
            _state->entity_streamer->get_desired_chunks(
                *_state->active_world,
                _state->chunk_loader->get_chunk_coords(*_state->active_world),
                dt,
                settings));
    }

    bool WorldManager::load_world_globals(World& world)
    {
        if (!world.globals.is_valid())
            return true;

        const auto asset_manager = _state->asset_manager.lock();
        if (!asset_manager)
            return false;

        const auto globals = asset_manager->load<WorldGlobals>(world.globals);
        if (!globals)
            return false;

        world.clear_runtime_entities();
        world.load_globals(*globals);
        asset_manager->set_pinned(world.globals, true);
        if (std::ranges::find(_state->loaded_globals, world.globals)
            == _state->loaded_globals.end())
        {
            _state->loaded_globals.push_back(world.globals);
        }
        return true;
    }

    void WorldManager::release_active_world()
    {
        if (_state->active_world)
            _state->chunk_loader->clear(*_state->active_world);

        if (!_state->active_world_from_asset || !_state->active_world_handle.is_valid())
            return;

        const auto asset_manager = _state->asset_manager.lock();
        if (!asset_manager)
            return;

        asset_manager->set_pinned(_state->active_world_handle, false);
        static_cast<void>(asset_manager->unload<World>(_state->active_world_handle, true));
    }
}
