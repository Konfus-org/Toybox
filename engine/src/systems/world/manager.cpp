#include "tbx/systems/world/manager.h"
#include "chunk_loader.h"
#include "streamer.h"
#include "tbx/interfaces/message_dispatcher.h"
#include "tbx/systems/debugging/macros.h"
#include <algorithm>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

namespace tbx
{
    //// INTERNAL ////

    struct WorldManager::State
    {
        State(
            std::weak_ptr<AssetManager> asset_manager,
            std::weak_ptr<IMessageCoordinator> message_coordinator)
            : asset_manager(std::move(asset_manager))
            , message_coordinator(std::move(message_coordinator))
            , chunk_loader(std::make_unique<ChunkLoader>(this->asset_manager))
            , entity_streamer(std::make_unique<EntityStreamer>())
        {
        }

        std::weak_ptr<AssetManager> asset_manager = {};
        std::weak_ptr<IMessageCoordinator> message_coordinator = {};
        std::unique_ptr<ChunkLoader> chunk_loader = {};
        std::unique_ptr<EntityStreamer> entity_streamer = {};
        std::shared_ptr<World> active_world = {};
        Handle active_world_handle = {};
        std::vector<Handle> loaded_globals = {};
        std::vector<Uuid> global_entities = {};
        Uuid reload_handler = {};
        bool active_world_from_asset = false;
    };

    //// WORLD MANAGER IMPL ////

    WorldManager::WorldManager(
        std::weak_ptr<AssetManager> asset_manager,
        std::weak_ptr<IMessageCoordinator> message_coordinator)
        : _state(std::make_unique<State>(std::move(asset_manager), std::move(message_coordinator)))
    {
        if (auto coordinator = _state->message_coordinator.lock())
        {
            _state->reload_handler = coordinator->register_handler(
                [this](Message& message)
                {
                    if (const auto reloaded = handle_message<AssetReloadedEvent>(message))
                        on_asset_reloaded(reloaded->get());
                });
        }
    }

    WorldManager::~WorldManager() noexcept
    {
        TBX_TRY_CATCH_ASSERT(
            {
                if (auto coordinator = _state->message_coordinator.lock())
                    coordinator->deregister_handler(_state->reload_handler);
                clear_active_world();
            },
            "Toybox world manager shutdown failed.");
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

        if (!_state->chunk_loader->set_chunks(*loaded_world))
            return false;
        if (!load_world_globals(*loaded_world))
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

    bool WorldManager::save_active_world()
    {
        if (!_state->active_world || !_state->active_world_from_asset)
            return false;

        const auto asset_manager = _state->asset_manager.lock();
        if (!asset_manager)
            return false;

        const auto serialization = asset_manager->get_serialization_registry().lock();
        if (!serialization)
            return false;

        auto& world = *_state->active_world;

        // Globals: every entity currently flagged global goes to the world's globals asset.
        if (world.globals.is_valid())
        {
            auto globals_asset = WorldGlobals {};
            for (const auto& entity : world.get_all())
            {
                if (world.is_global(entity.get_id()))
                    globals_asset.entities.absorb(entity);
            }

            const auto path = asset_manager->resolve_path(world.globals);
            if (path.empty() || !serialization->write(path, globals_asset))
                return false;
        }

        // Chunks: each loaded chunk keeps its own entities; entities created since load that belong to no
        // chunk are folded into the primary (lowest-coord) chunk so they persist.
        const auto loaded_chunks = _state->chunk_loader->get_loaded_chunks(world);
        if (loaded_chunks.empty())
            return true;

        auto assigned = std::unordered_set<Uuid> {};
        for (const auto& chunk : loaded_chunks)
            assigned.insert(chunk.entities.begin(), chunk.entities.end());

        const auto primary = std::ranges::min_element(
            loaded_chunks,
            [](const LoadedChunkInfo& left, const LoadedChunkInfo& right)
            {
                return std::tie(left.coord.x, left.coord.y, left.coord.z)
                       < std::tie(right.coord.x, right.coord.y, right.coord.z);
            });

        for (const auto& chunk : loaded_chunks)
        {
            auto chunk_asset = WorldChunk {};
            chunk_asset.coord = chunk.coord;

            for (const auto& id : chunk.entities)
            {
                if (world.has(id) && !world.is_global(id))
                    chunk_asset.entities.absorb(world.get(id));
            }

            // The primary chunk also takes in any new, still-ungrouped runtime entities.
            if (&chunk == &*primary)
            {
                for (const auto& entity : world.get_all())
                {
                    const auto id = entity.get_id();
                    if (!world.is_global(id) && !assigned.contains(id))
                        chunk_asset.entities.absorb(entity);
                }
            }

            const auto path = asset_manager->resolve_path(chunk.handle);
            if (path.empty() || !serialization->write(path, chunk_asset))
                return false;
        }

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

        world.load_globals(*globals);
        _state->global_entities = collect_entity_ids(globals->entities);
        asset_manager->set_pinned(world.globals, true);
        if (std::ranges::find(_state->loaded_globals, world.globals)
            == _state->loaded_globals.end())
        {
            _state->loaded_globals.push_back(world.globals);
        }
        return true;
    }

    std::vector<Entity> WorldManager::collect_runtime_entities(
        const World& world,
        const std::vector<Uuid>& asset_entity_ids)
    {
        auto asset_entities = std::unordered_set<Uuid>();
        asset_entities.insert(asset_entity_ids.begin(), asset_entity_ids.end());

        auto entities = std::vector<Entity>();
        for (const auto& entity : world.get_all())
        {
            if (!asset_entities.contains(entity.get_id()))
                entities.push_back(entity);
        }

        return entities;
    }

    bool WorldManager::refresh_active_world_from_asset()
    {
        if (!_state->active_world || !_state->active_world_from_asset
            || !_state->active_world_handle.id.is_valid())
        {
            return false;
        }

        const auto asset_manager = _state->asset_manager.lock();
        if (!asset_manager)
            return false;

        auto asset_entity_ids = _state->global_entities;
        const auto chunk_entities =
            _state->chunk_loader->get_loaded_chunk_entity_ids(*_state->active_world);
        asset_entity_ids.insert(
            asset_entity_ids.end(),
            chunk_entities.begin(),
            chunk_entities.end());

        const auto runtime_entities =
            collect_runtime_entities(*_state->active_world, asset_entity_ids);
        const auto loaded_chunks =
            _state->chunk_loader->get_loaded_chunk_handles(*_state->active_world);

        auto loaded_world = asset_manager->load<World>(_state->active_world_handle);
        if (!loaded_world)
            return false;

        if (!_state->chunk_loader->set_chunks(*loaded_world))
            return false;
        if (!load_world_globals(*loaded_world))
            return false;

        _state->chunk_loader->load_chunks(*loaded_world, loaded_chunks);
        loaded_world->add_entities(runtime_entities);

        if (_state->active_world)
            _state->chunk_loader->clear(*_state->active_world);

        _state->active_world = std::move(loaded_world);
        return true;
    }

    bool WorldManager::refresh_world_chunk(const Handle& chunk)
    {
        if (!_state->active_world || !chunk.id.is_valid())
            return false;

        return _state->chunk_loader->refresh_loaded_chunk(*_state->active_world, chunk);
    }

    bool WorldManager::refresh_world_globals()
    {
        if (!_state->active_world || !_state->active_world->globals.id.is_valid())
            return false;

        _state->active_world->remove_entities(_state->global_entities);
        _state->global_entities.clear();
        return load_world_globals(*_state->active_world);
    }

    void WorldManager::on_asset_reloaded(const AssetReloadedEvent& event)
    {
        if (!event.succeeded || !_state->active_world)
            return;

        const auto changed_asset = event.affected_asset.id;
        if (_state->active_world_from_asset && changed_asset == _state->active_world_handle.id)
        {
            refresh_active_world_from_asset();
            return;
        }

        if (changed_asset == _state->active_world->globals.id)
        {
            refresh_world_globals();
            return;
        }

        refresh_world_chunk(event.affected_asset);
    }

    void WorldManager::release_active_world()
    {
        if (_state->active_world)
            _state->chunk_loader->clear(*_state->active_world);

        const auto asset_manager = _state->asset_manager.lock();
        if (!asset_manager)
        {
            _state->global_entities.clear();
            return;
        }

        for (const auto& globals : _state->loaded_globals)
            asset_manager->set_pinned(globals, false);
        _state->loaded_globals.clear();

        if (!_state->active_world_from_asset || !_state->active_world_handle.is_valid())
        {
            _state->global_entities.clear();
            return;
        }

        asset_manager->set_pinned(_state->active_world_handle, false);
        asset_manager->unload<World>(_state->active_world_handle, true);
        _state->global_entities.clear();
    }
}
