#include "tbx/systems/world/manager.h"
#include "chunk_loader.h"
#include "streamer.h"
#include "tbx/interfaces/message_dispatcher.h"
#include "tbx/systems/async/thread_manager.h"
#include "tbx/systems/debugging/macros.h"
#include <algorithm>
#include <mutex>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace tbx
{
    //// INTERNAL ////

    // The dedicated ThreadManager lane that deserializes world chunks off the main thread.
    static constexpr auto STREAMING_LANE = std::string_view("streaming");

    // Hand-off between the streaming lane (producer) and the main thread (consumer): chunk assets the
    // lane has finished deserializing, and the set of chunks currently being loaded so a coord isn't
    // posted twice. Held by shared_ptr and co-owned by each posted lane task, so an in-flight task can
    // safely push its result even if it outlives the WorldManager (it won't — stop_all() joins the
    // lane before teardown — but co-ownership makes the lifetime correct regardless).
    struct StreamingChannel
    {
        std::mutex mutex = {};
        std::vector<std::pair<IVec3, std::shared_ptr<WorldChunk>>> ready = {};
        std::unordered_set<IVec3> in_flight = {};
    };

    // A standalone world loaded via open_world(..., STANDALONE): a live World kept apart from the active world
    // (e.g. an editor asset-preview world), plus the globals bookkeeping needed to release it cleanly.
    // Its source asset handle is NOT pinned — the instance owns its own World copy, so the cached template
    // is free to unload; only the globals it loaded are pinned (and unpinned on close).
    struct WorldInstance
    {
        std::shared_ptr<World> world = {};
        Handle handle = {};
        std::vector<Uuid> global_entities = {};
        std::vector<Handle> loaded_globals = {};
    };

    // An additive layer loaded via open_world(..., ADDITIVE): a world loaded on top of the active world. Its
    // entities are injected into the active world (so they render/simulate with it) but tracked here by their
    // ids so the layer can be removed again, and excluded when the active world is saved. `world` is the
    // isolated copy the entities were loaded through (its id keys the layer and is the close_world handle);
    // it owns its own chunk records and pins its globals until the layer closes.
    struct AdditiveLayer
    {
        std::shared_ptr<World> world = {};
        Handle handle = {};
        std::vector<Uuid> entity_ids = {}; // injected into the active world; removed on close
        std::vector<Handle> loaded_globals = {};
    };

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
        std::weak_ptr<ThreadManager> thread_manager = {};
        std::unique_ptr<ChunkLoader> chunk_loader = {};
        std::unique_ptr<EntityStreamer> entity_streamer = {};
        std::shared_ptr<StreamingChannel> streaming = std::make_shared<StreamingChannel>();
        bool streaming_lane_ready = false;
        bool streaming_enabled = true; // false => keep every chunk loaded (editor; no view culling)
        std::shared_ptr<World> active_world = {};
        Handle active_world_handle = {};
        std::vector<Handle> loaded_globals = {};
        std::vector<Uuid> global_entities = {};
        Uuid reload_handler = {};
        bool active_world_from_asset = false;
        std::unordered_map<Uuid, WorldInstance> instances = {};      // standalone worlds, keyed by their id
        std::unordered_map<Uuid, AdditiveLayer> additive_layers = {}; // layers on the active world, by their id
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
                for (auto& [id, instance] : _state->instances)
                    release_instance(instance.world.get(), instance.loaded_globals);
                _state->instances.clear();
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

    std::shared_ptr<World> WorldManager::load_isolated_world(
        const World& tmpl, std::vector<Uuid>& out_globals, std::vector<Handle>& out_loaded)
    {
        // load<World> hands back one shared cached instance per handle, and every chunk/globals record keys on
        // world.id — so an isolated world (a standalone instance or the copy an additive layer loads through)
        // must be its OWN World with a fresh unique id, or two of the same asset (or one and the active world)
        // would collide. Copy the template's description into a fresh instance; the cached template is then
        // free to unload (we never pin the source handle). Populate every chunk up front — these worlds don't
        // stream, so a describe/render right after open sees the whole world.
        auto instance = std::make_shared<World>();
        instance->id = Uuid::generate();
        instance->globals = tmpl.globals;
        instance->chunks = tmpl.chunks;
        instance->add_entities(tmpl.get_all());

        if (!_state->chunk_loader->set_chunks(*instance)
            || !load_world_globals(*instance, out_globals, out_loaded))
        {
            release_instance(instance.get(), out_loaded);
            return {};
        }

        load_all_chunks(*instance);
        return instance;
    }

    std::shared_ptr<World> WorldManager::open_world(const Handle& handle, WorldOpenMode mode)
    {
        if (!handle.is_valid())
            return {};

        const auto asset_manager = _state->asset_manager.lock();
        if (!asset_manager)
            return {};

        if (mode == WorldOpenMode::ADDITIVE && !_state->active_world)
        {
            TBX_TRACE_WARNING("Cannot additively load '{}': there is no active world to load on top of.", handle);
            return {};
        }

        if (mode == WorldOpenMode::REPLACE && _state->active_world && _state->active_world_from_asset
            && _state->active_world_handle == handle)
            return _state->active_world;

        const auto loaded = asset_manager->load<World>(handle);
        if (!loaded)
            return {};

        if (mode == WorldOpenMode::STANDALONE)
        {
            auto record = WorldInstance {};
            record.handle = handle;
            auto instance = load_isolated_world(*loaded, record.global_entities, record.loaded_globals);
            if (!instance)
                return {};

            record.world = instance;
            _state->instances.emplace(instance->id, std::move(record));
            return instance;
        }

        if (mode == WorldOpenMode::ADDITIVE)
        {
            // Load the world through an isolated copy (fresh id, fully loaded), then inject its entities into
            // the active world so they render/simulate with it. The copy is kept as the layer's handle: its id
            // keys the layer for close_world, and it owns the chunk records + globals pin the layer releases.
            auto layer = AdditiveLayer {};
            layer.handle = handle;
            auto injected_globals = std::vector<Uuid> {};
            auto instance = load_isolated_world(*loaded, injected_globals, layer.loaded_globals);
            if (!instance)
                return {};

            const auto entities = instance->get_all();
            _state->active_world->add_entities(entities);
            layer.world = instance;
            layer.entity_ids.reserve(entities.size());
            for (const auto& entity : entities)
                layer.entity_ids.push_back(entity.get_id());

            _state->additive_layers.emplace(instance->id, std::move(layer));
            return instance;
        }

        // REPLACE: reuse the cached instance directly and pin its handle (single active world, may stream).
        if (!_state->chunk_loader->set_chunks(*loaded))
            return {};
        if (!load_world_globals(*loaded))
            return {};

        release_active_world();
        asset_manager->set_pinned(handle, true);
        _state->active_world = loaded;
        _state->active_world_handle = handle;
        _state->active_world_from_asset = true;
        if (!_state->streaming_enabled)
            load_all_chunks(*_state->active_world);
        return _state->active_world;
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

        // Additive layers are loaded on top of the base world but are not part of it, so their injected
        // entities must never be written back into the base world's globals or chunks.
        auto additive_entities = std::unordered_set<Uuid> {};
        for (const auto& [id, layer] : _state->additive_layers)
            additive_entities.insert(layer.entity_ids.begin(), layer.entity_ids.end());

        // Globals: every entity currently flagged global goes to the world's globals asset.
        if (world.globals.is_valid())
        {
            auto globals_asset = WorldGlobals {};
            for (const auto& entity : world.get_all())
            {
                if (world.is_global(entity.get_id()) && !additive_entities.contains(entity.get_id()))
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
                if (world.has(id) && !world.is_global(id) && !additive_entities.contains(id))
                    chunk_asset.entities.absorb(world.get(id));
            }

            // The primary chunk also takes in any new, still-ungrouped runtime entities.
            if (&chunk == &*primary)
            {
                for (const auto& entity : world.get_all())
                {
                    const auto id = entity.get_id();
                    if (!world.is_global(id) && !assigned.contains(id) && !additive_entities.contains(id))
                        chunk_asset.entities.absorb(entity);
                }
            }

            const auto path = asset_manager->resolve_path(chunk.handle);
            if (path.empty() || !serialization->write(path, chunk_asset))
                return false;
        }

        return true;
    }

    void WorldManager::set_thread_manager(std::weak_ptr<ThreadManager> thread_manager)
    {
        _state->thread_manager = std::move(thread_manager);
    }

    void WorldManager::set_streaming_enabled(bool enabled)
    {
        _state->streaming_enabled = enabled;
    }

    void WorldManager::update(const DeltaTime&, const WorldSettings& settings)
    {
        if (!_state->active_world)
            return;

        World& world = *_state->active_world;
        auto& chunk_loader = *_state->chunk_loader;
        auto& streamer = *_state->entity_streamer;

        // 1. Integrate chunks the streaming lane finished deserializing (main-thread ECS mutation).
        drain_streamed_chunks(world);

        // 2. Decide which chunks are wanted from the camera view(s). A chunk is wanted when its cached
        // world bounds are visible to a camera (the same frustum test the renderer culls with) or
        // within the keep-loaded bubble. A chunk whose bounds aren't known yet (never loaded) is
        // wanted so it loads once and caches them; with no camera at all (or streaming disabled, e.g.
        // the editor) we keep everything.
        auto views = std::vector<StreamerCameraView> {};
        if (_state->streaming_enabled)
            views = streamer.collect_views(world);
        auto desired = std::unordered_set<IVec3> {};
        for (const auto& coord : chunk_loader.get_chunk_coords(world))
        {
            auto wanted = true;
            if (!views.empty())
            {
                if (const auto bounds = chunk_loader.get_chunk_bounds(world, coord))
                    wanted = streamer.is_chunk_visible(views, *bounds, settings.keep_loaded_radius);
            }
            if (!wanted)
                continue;

            desired.insert(coord);
            // 3. Stream in wanted chunks that aren't loaded yet (async on the streaming lane).
            if (!chunk_loader.is_chunk_loaded(world, coord))
                request_chunk_load(world, coord);
        }

        // 4. Stream out loaded chunks that have left the view.
        for (const auto& coord : chunk_loader.get_loaded_coords(world))
            if (!desired.contains(coord))
                chunk_loader.unload_chunk(world, coord);
    }

    void WorldManager::drain_streamed_chunks(World& world)
    {
        auto ready = std::vector<std::pair<IVec3, std::shared_ptr<WorldChunk>>> {};
        {
            std::lock_guard lock(_state->streaming->mutex);
            ready.swap(_state->streaming->ready);
        }

        for (auto& [coord, chunk] : ready)
        {
            _state->chunk_loader->integrate_loaded(world, coord, chunk);
            std::lock_guard lock(_state->streaming->mutex);
            _state->streaming->in_flight.erase(coord);
        }
    }

    void WorldManager::request_chunk_load(World& world, const IVec3& coord)
    {
        const Handle handle = _state->chunk_loader->get_chunk_handle(world, coord);
        if (!handle.id.is_valid() && handle.name.empty())
            return;

        // Async streaming only when enabled (the running game). When disabled (the editor) chunks load
        // synchronously below so the world is fully populated immediately — the editor describes the
        // world right after activating it and must see every streamed entity, not wait for a lane.
        if (_state->streaming_enabled)
            if (auto thread_manager = _state->thread_manager.lock();
                thread_manager && ensure_streaming_lane(*thread_manager))
            {
                {
                    std::lock_guard lock(_state->streaming->mutex);
                    if (_state->streaming->in_flight.contains(coord))
                        return; // already streaming in
                    _state->streaming->in_flight.insert(coord);
                }

                // Lane task: pure asset I/O (the AssetManager is internally synchronized), then push
                // the deserialized chunk onto the shared channel for the main thread to integrate.
                // Captures the channel by shared_ptr so its lifetime never depends on teardown order.
                auto asset_manager = _state->asset_manager;
                auto channel = _state->streaming;
                thread_manager->post(
                    STREAMING_LANE,
                    [asset_manager, channel, handle, coord]
                    {
                        auto manager = asset_manager.lock();
                        auto chunk = manager ? manager->load<WorldChunk>(handle)
                                             : std::shared_ptr<WorldChunk> {};
                        std::lock_guard lock(channel->mutex);
                        channel->ready.emplace_back(coord, std::move(chunk));
                    });
                return;
            }

        // Streaming disabled, or no lane (headless / tests): load synchronously on this thread.
        if (const auto asset_manager = _state->asset_manager.lock())
            _state->chunk_loader->integrate_loaded(
                world,
                coord,
                asset_manager->load<WorldChunk>(handle));
    }

    void WorldManager::load_all_chunks(World& world)
    {
        // With streaming disabled (editor), request_chunk_load is synchronous, so this populates the
        // whole world immediately — used right after activation so a describe sees every entity.
        for (const auto& coord : _state->chunk_loader->get_chunk_coords(world))
            if (!_state->chunk_loader->is_chunk_loaded(world, coord))
                request_chunk_load(world, coord);
    }

    bool WorldManager::ensure_streaming_lane(ThreadManager& thread_manager)
    {
        if (_state->streaming_lane_ready)
            return true;

        _state->streaming_lane_ready =
            thread_manager.has_lane(STREAMING_LANE) || thread_manager.try_create_lane(STREAMING_LANE);
        return _state->streaming_lane_ready;
    }

    bool WorldManager::load_world_globals(World& world)
    {
        return load_world_globals(world, _state->global_entities, _state->loaded_globals);
    }

    bool WorldManager::load_world_globals(
        World& world, std::vector<Uuid>& out_globals, std::vector<Handle>& out_loaded)
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
        out_globals = collect_entity_ids(globals->entities);
        asset_manager->set_pinned(world.globals, true);
        if (std::ranges::find(out_loaded, world.globals) == out_loaded.end())
            out_loaded.push_back(world.globals);
        return true;
    }

    void WorldManager::close_world(const Uuid& world_id)
    {
        if (const auto layer = _state->additive_layers.find(world_id);
            layer != _state->additive_layers.end())
        {
            if (_state->active_world)
                _state->active_world->remove_entities(layer->second.entity_ids);
            release_instance(layer->second.world.get(), layer->second.loaded_globals);
            _state->additive_layers.erase(layer);
            return;
        }

        const auto it = _state->instances.find(world_id);
        if (it == _state->instances.end())
            return;

        release_instance(it->second.world.get(), it->second.loaded_globals);
        _state->instances.erase(it);
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
        // Additive layers live on the active world; drop them with it. Their injected entities vanish when the
        // active world is cleared below, so we only release each layer's isolated copy (its chunk records + its
        // globals pin) — no per-entity removal needed here.
        for (auto& [id, layer] : _state->additive_layers)
            release_instance(layer.world.get(), layer.loaded_globals);
        _state->additive_layers.clear();

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

    void WorldManager::release_instance(World* world, const std::vector<Handle>& loaded_globals)
    {
        // Clear the instance's chunk records (keyed by its unique id) and unpin the globals it loaded. The
        // source .world handle was never pinned (the instance owns its own World copy), so — unlike the
        // active world — there is nothing to unpin/unload for it here.
        if (world)
            _state->chunk_loader->clear(*world);

        const auto asset_manager = _state->asset_manager.lock();
        if (!asset_manager)
            return;

        for (const auto& globals : loaded_globals)
            asset_manager->set_pinned(globals, false);
    }
}
