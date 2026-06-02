#include "tbx/systems/scripting/script_system.h"
#include "script_system_state_key.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/types/assets/world.h"
#include "tbx/types/components/script_container.h"
#include <algorithm>
#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace tbx
{
    struct ScriptSystemStateRecord
    {
        std::shared_ptr<Script> script = {};
        bool started = false;
        bool touched = false;
    };

    struct ScriptSystem::State
    {
        static ScriptSystemStateKey make_key(
            const World& world,
            const Entity& entity,
            const ScriptContainerBinding& binding);

        std::unordered_map<ScriptSystemStateKey, ScriptSystemStateRecord> instances = {};
        std::unordered_set<Uuid> pending_script_reloads = {};
        Uuid reload_handler = {};
    };

    ScriptSystemStateKey ScriptSystem::State::make_key(
        const World& world,
        const Entity& entity,
        const ScriptContainerBinding& binding)
    {
        return ScriptSystemStateKey {
            .world = world.id,
            .entity = entity.get_id(),
            .script = binding.script,
            .binding_id = binding.binding_id,
        };
    }

    ScriptSystem::ScriptSystem(
        std::weak_ptr<AssetManager> asset_manager,
        ServiceProvider& services,
        std::weak_ptr<WorldManager> world_manager,
        std::weak_ptr<AssetReloadQueue> reload_queue)
        : _state(std::make_unique<State>())
        , _asset_manager(std::move(asset_manager))
        , _reload_queue(std::move(reload_queue))
        , _world_manager(std::move(world_manager))
        , _services(services)
    {
        if (auto queue = _reload_queue.lock())
        {
            _state->reload_handler = queue->register_handler(
                [this](const AssetReloadContext& context)
                {
                    on_asset_reload(context);
                });
        }
    }

    ScriptSystem::~ScriptSystem() noexcept
    {
        if (auto queue = _reload_queue.lock())
            queue->deregister_handler(_state->reload_handler);

        for (auto& entry : _state->instances)
        {
            if (entry.second.script)
                entry.second.script->on_destroy();
        }
        _state->instances.clear();
    }

    static std::shared_ptr<Script> clone_script_prototype(const Script& prototype)
    {
        auto registration = get_asset_type_registration(std::type_index(typeid(prototype)));
        if (!registration.has_value() || !registration->create_asset)
            return {};

        auto asset = registration->create_asset();
        if (!asset)
            return {};

        if (registration->write_body && registration->read_body)
        {
            auto data = std::string();
            auto write_result = registration->write_body(&prototype, data);
            if (!write_result.succeeded())
                return {};

            auto read_result = registration->read_body(data, asset.get());
            if (!read_result.succeeded())
                return {};
        }

        asset->id = prototype.id;
        asset->version = prototype.version;
        auto* script = dynamic_cast<Script*>(asset.release());
        if (script == nullptr)
            return {};

        return std::shared_ptr<Script>(script);
    }

    static std::shared_ptr<Script> create_script_instance(
        AssetManager& asset_manager,
        const ScriptContainerBinding& binding)
    {
        auto prototype_asset = asset_manager.load(Handle(binding.script));
        auto prototype = std::dynamic_pointer_cast<Script>(prototype_asset);
        if (!prototype)
        {
            TBX_TRACE_WARNING("Failed to load script asset id={}.", binding.script);
            return {};
        }

        auto instance = clone_script_prototype(*prototype);
        if (!instance)
        {
            TBX_TRACE_WARNING("Failed to create script instance id={}.", binding.script);
            return {};
        }

        auto* instance_ptr = instance.get();
        auto registration = get_asset_type_registration(std::type_index(typeid(*instance_ptr)));
        if (registration.has_value() && registration->apply_overrides
            && !binding.overrides.is_null() && !binding.overrides.empty())
        {
            auto result = registration->apply_overrides(binding.overrides, instance.get());
            if (!result.succeeded())
            {
                TBX_TRACE_WARNING(
                    "Failed to apply script overrides id={}: {}",
                    binding.script,
                    result.get_report());
                return {};
            }
        }

        return instance;
    }

    static std::vector<std::shared_ptr<World>> get_script_worlds(
        AssetManager& asset_manager,
        const std::weak_ptr<WorldManager>& world_manager)
    {
        if (const auto manager = world_manager.lock())
        {
            if (auto world = manager->get_active_world().lock())
                return {world};

            return {};
        }

        return asset_manager.get_loaded<World>();
    }

    void ScriptSystem::fixed_update(const DeltaTime& dt)
    {
        auto asset_manager = _asset_manager.lock();
        if (!asset_manager)
            return;

        consume_script_reloads();

        for (auto& entry : _state->instances)
            entry.second.touched = false;

        const auto worlds = get_script_worlds(*asset_manager, _world_manager);
        for (const auto& world : worlds)
        {
            if (!world)
                continue;

            world->for_each_with<ScriptContainer>(
                [this, &asset_manager, &dt, &world](Entity& entity)
                {
                    auto& container = entity.get_component<ScriptContainer>();
                    for (const auto& binding : container.scripts)
                    {
                        if (!binding.enabled || !binding.script.is_valid())
                            continue;

                        const auto key = State::make_key(*world, entity, binding);
                        auto& record = _state->instances[key];
                        record.touched = true;
                        if (!record.script)
                            record.script = create_script_instance(*asset_manager, binding);
                        if (!record.script)
                            continue;

                        record.script->bind(
                            ScriptBinding {
                                .entity = entity.get_id(),
                                .script = binding.script,
                                .binding_id = binding.binding_id,
                            });
                        auto context =
                            ScriptContext(world->id, entity, world, _services.get(), *this);
                        record.script->bind_context(context);
                        auto* script_ptr = record.script.get();
                        if (auto registration =
                                get_asset_type_registration(std::type_index(typeid(*script_ptr)));
                            registration.has_value() && registration->bind_runtime)
                        {
                            registration->bind_runtime(record.script.get(), context);
                        }

                        if (!record.started)
                        {
                            record.script->on_start();
                            record.started = true;
                        }
                        record.script->on_fixed_update(dt);
                    }
                });
        }
    }

    std::weak_ptr<Script> ScriptSystem::try_get_script(const ScriptLookup& lookup)
    {
        if (lookup.binding_id.is_valid())
        {
            auto iterator = _state->instances.find(
                ScriptSystemStateKey {
                    .world = lookup.world,
                    .entity = lookup.entity,
                    .script = lookup.script,
                    .binding_id = lookup.binding_id,
                });
            return iterator == _state->instances.end() ? std::weak_ptr<Script> {}
                                                       : iterator->second.script;
        }

        const auto iterator = std::ranges::find_if(
            _state->instances,
            [&lookup](const auto& entry)
            {
                return entry.first.world == lookup.world && entry.first.entity == lookup.entity
                       && entry.first.script == lookup.script;
            });
        return iterator == _state->instances.end() ? std::weak_ptr<Script> {}
                                                   : iterator->second.script;
    }

    void ScriptSystem::update(const DeltaTime& dt)
    {
        auto asset_manager = _asset_manager.lock();
        if (!asset_manager)
            return;

        consume_script_reloads();

        for (auto& entry : _state->instances)
            entry.second.touched = false;

        const auto worlds = get_script_worlds(*asset_manager, _world_manager);
        for (const auto& world : worlds)
        {
            if (!world)
                continue;

            world->for_each_with<ScriptContainer>(
                [this, &asset_manager, &dt, &world](Entity& entity)
                {
                    auto& container = entity.get_component<ScriptContainer>();
                    for (const auto& binding : container.scripts)
                    {
                        if (!binding.enabled || !binding.script.is_valid())
                            continue;

                        const auto key = State::make_key(*world, entity, binding);
                        auto& record = _state->instances[key];
                        record.touched = true;
                        if (!record.script)
                            record.script = create_script_instance(*asset_manager, binding);
                        if (!record.script)
                            continue;

                        record.script->bind(
                            ScriptBinding {
                                .entity = entity.get_id(),
                                .script = binding.script,
                                .binding_id = binding.binding_id,
                            });
                        auto context =
                            ScriptContext(world->id, entity, world, _services.get(), *this);
                        record.script->bind_context(context);
                        auto* script_ptr = record.script.get();
                        if (auto registration =
                                get_asset_type_registration(std::type_index(typeid(*script_ptr)));
                            registration.has_value() && registration->bind_runtime)
                        {
                            registration->bind_runtime(record.script.get(), context);
                        }

                        if (!record.started)
                        {
                            record.script->on_start();
                            record.started = true;
                        }
                        record.script->on_update(dt);
                    }
                });
        }

        auto stale_keys = std::vector<ScriptSystemStateKey> {};
        for (const auto& entry : _state->instances)
        {
            if (!entry.second.touched)
                stale_keys.push_back(entry.first);
        }

        for (const auto& key : stale_keys)
        {
            if (auto iterator = _state->instances.find(key); iterator != _state->instances.end())
            {
                if (iterator->second.script)
                    iterator->second.script->on_destroy();
                _state->instances.erase(iterator);
            }
        }
    }

    void ScriptSystem::consume_script_reloads()
    {
        if (_state->pending_script_reloads.empty())
            return;

        for (auto& entry : _state->instances)
        {
            if (!_state->pending_script_reloads.contains(entry.first.script))
                continue;

            if (entry.second.script)
                entry.second.script->on_destroy();

            entry.second.script = {};
            entry.second.started = false;
        }
        _state->pending_script_reloads.clear();
    }

    void ScriptSystem::on_asset_reload(const AssetReloadContext& context)
    {
        if (!context.succeeded || !context.affected_asset.id.is_valid())
            return;

        _state->pending_script_reloads.insert(context.affected_asset.id);
    }
}
