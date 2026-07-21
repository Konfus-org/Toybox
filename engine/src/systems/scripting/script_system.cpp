#include "tbx/systems/scripting/script_system.h"
#include "script_system_state_key.h"
#include "tbx/interfaces/message_dispatcher.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/plugin_api/messages.h"
#include "tbx/systems/scripting/script.h"
#include "tbx/systems/scripting/script_registry.h"
#include "tbx/types/assets/world.h"
#include "tbx/types/components/script_container.h"
#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace tbx
{
    static std::shared_ptr<ServiceProvider> make_non_owning_service_provider(
        ServiceProvider& services)
    {
        return std::shared_ptr<ServiceProvider>(
            &services,
            [](ServiceProvider*)
            {
            });
    }

    // Deep-copies a script prototype by routing it through its registered body serializer, so a fresh
    // instance starts from the asset's authored default values before per-binding overrides apply.
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
            if (!registration->write_body(&prototype, data).succeeded())
                return {};
            if (!registration->read_body(data, asset.get()).succeeded())
                return {};
        }

        asset->id = prototype.id;
        asset->version = prototype.version;
        auto* script = dynamic_cast<Script*>(asset.release());
        if (script == nullptr)
            return {};

        return std::shared_ptr<Script>(script);
    }

    // Resolves the script registration for a live instance from its concrete asset type.
    static const ScriptRegistration* script_registration_for(const Script& script)
    {
        const auto asset_registration = get_asset_type_registration(std::type_index(typeid(script)));
        return asset_registration.has_value() ? get_script_registration(asset_registration->type_name)
                                              : nullptr;
    }

    // Loads a script asset, clones its authored prototype, and applies the binding's stored overrides.
    // Returns null when the asset is not a script (or fails to clone), leaving the binding uninstantiated.
    static std::shared_ptr<Script> instantiate_script(
        AssetManager& asset_manager,
        const Handle& script,
        const Json& overrides)
    {
        auto prototype = std::dynamic_pointer_cast<Script>(asset_manager.load(script));
        if (!prototype)
            return {};

        auto instance = clone_script_prototype(*prototype);
        if (!instance)
        {
            TBX_TRACE_WARNING("Failed to create script instance id={}.", script.id);
            return {};
        }

        if (const auto* registration = script_registration_for(*instance);
            registration != nullptr && registration->apply_overrides && !overrides.is_null()
            && !overrides.empty())
        {
            if (auto result = registration->apply_overrides(overrides, instance.get());
                !result.succeeded())
            {
                TBX_TRACE_WARNING(
                    "Failed to apply script overrides id={}: {}", script.id, result.get_report());
                return {};
            }
        }

        return instance;
    }

    // Binds the runtime context onto a live instance, then runs the type's generated bind_runtime
    // (resolving injected services and script references).
    static void bind_script(Script& script, ScriptContext& context)
    {
        script.bind(context);
        if (const auto* registration = script_registration_for(script);
            registration != nullptr && registration->bind_runtime)
            registration->bind_runtime(&script, context);
    }

    struct ScriptSystemStateRecord
    {
        std::shared_ptr<Script> instance = {};
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
            .script = binding.script.id,
            .binding_id = binding.binding_id,
        };
    }

    ScriptSystem::ScriptSystem(std::shared_ptr<AssetManager> asset_manager, ServiceProvider& services)
        : _state(std::make_unique<State>())
        , _asset_manager(std::move(asset_manager))
        , _service_provider_alias(make_non_owning_service_provider(services))
        , _services(_service_provider_alias)
    {
    }

    ScriptSystem::ScriptSystem(
        std::weak_ptr<ServiceProvider> services,
        std::weak_ptr<AssetManager> asset_manager,
        std::weak_ptr<WorldManager> world_manager,
        std::weak_ptr<IMessageCoordinator> message_coordinator)
        : _state(std::make_unique<State>())
        , _asset_manager(std::move(asset_manager))
        , _message_coordinator(message_coordinator)
        , _world_manager(std::move(world_manager))
        , _services(services)
    {
        if (auto coordinator = _message_coordinator.lock())
        {
            _state->reload_handler = coordinator->register_handler(
                [this](Message& message)
                {
                    if (const auto reloaded = handle_message<AssetReloadedEvent>(message))
                        on_asset_reloaded(reloaded->get());
                    else if (handle_message<PluginUnloadingEvent>(message))
                        on_plugins_unloading();
                });
        }
    }

    ScriptSystem::~ScriptSystem() noexcept
    {
        if (auto coordinator = _message_coordinator.lock())
            coordinator->deregister_handler(_state->reload_handler);

        for (auto& entry : _state->instances)
        {
            if (entry.second.instance)
                entry.second.instance->on_destroy();
        }
        _state->instances.clear();
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

    void ScriptSystem::update(const DeltaTime& dt, bool fixed)
    {
        auto services = _services.lock();
        auto asset_manager = _asset_manager.lock();
        if (!services || !asset_manager)
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
                [this, &asset_manager, &dt, &world, &services, fixed](Entity& entity)
                {
                    auto& container = entity.get_component<ScriptContainer>();
                    for (const auto& binding : container.scripts)
                    {
                        if (!binding.enabled || !binding.script.id.is_valid())
                            continue;

                        const auto key = State::make_key(*world, entity, binding);
                        auto& record = _state->instances[key];
                        record.touched = true;
                        if (!record.instance)
                            record.instance = instantiate_script(
                                *asset_manager, binding.script, binding.overrides);
                        if (!record.instance)
                            continue;

                        const auto script_binding = ScriptBinding {
                            .entity = entity.get_id(),
                            .script = binding.script.id,
                            .binding_id = binding.binding_id,
                        };
                        auto context = ScriptContext(
                            world->id,
                            script_binding,
                            entity,
                            world,
                            *services,
                            *this);
                        bind_script(*record.instance, context);

                        if (!record.started)
                        {
                            record.instance->on_start();
                            record.started = true;
                        }
                        if (fixed)
                            record.instance->on_fixed_update(dt);
                        else
                            record.instance->on_update(dt);
                    }
                });
        }

        if (fixed)
            return;

        // Only the variable-rate pass reaps: destroy instances whose bindings disappeared this frame.
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
                if (iterator->second.instance)
                    iterator->second.instance->on_destroy();
                _state->instances.erase(iterator);
            }
        }
    }

    Result ScriptSystem::apply_overrides(const ScriptLookup& lookup, const Json& overrides)
    {
        const auto iterator = _state->instances.find(
            ScriptSystemStateKey {
                .world = lookup.world,
                .entity = lookup.entity,
                .script = lookup.script,
                .binding_id = lookup.binding_id,
            });
        if (iterator == _state->instances.end() || !iterator->second.instance)
            return Result();

        // A live instance IS the Script, so a mid-play tweak routes through the same generated per-type
        // apply the initial instantiate used — fields present in the json land on the instance,
        // everything else keeps its running state.
        auto& script = *iterator->second.instance;
        const auto* registration = script_registration_for(script);
        if (registration == nullptr || registration->apply_overrides == nullptr)
            return Result(false, "Script type has no override registration.");

        if (overrides.is_null() || overrides.empty())
            return Result();

        return registration->apply_overrides(overrides, &script);
    }

    void ScriptSystem::reset()
    {
        // Tear down every live instance (running its on_destroy) and forget it, so the next update
        // re-instantiates the bindings from scratch — on_start runs again and no per-instance state
        // survives. Instances are keyed by entity + binding id, both preserved across a world restore,
        // so without this an entity that kept its id would silently resume its previous play's state.
        for (auto& entry : _state->instances)
        {
            if (entry.second.instance)
                entry.second.instance->on_destroy();
        }
        _state->instances.clear();
    }

    void ScriptSystem::fixed_update(const DeltaTime& dt)
    {
        update(dt, true);
    }

    void ScriptSystem::update(const DeltaTime& dt)
    {
        update(dt, false);
    }

    std::weak_ptr<Script> ScriptSystem::try_get_script(const ScriptLookup& lookup)
    {
        if (lookup.binding_id.is_valid())
        {
            if (lookup.script.is_valid())
            {
                auto iterator = _state->instances.find(
                    ScriptSystemStateKey {
                        .world = lookup.world,
                        .entity = lookup.entity,
                        .script = lookup.script,
                        .binding_id = lookup.binding_id,
                    });
                return iterator == _state->instances.end() ? std::weak_ptr<Script> {}
                                                           : iterator->second.instance;
            }

            // A binding id alone identifies the instance (ids are unique per world); a reference
            // that didn't carry the script asset id still resolves.
            const auto by_binding = std::ranges::find_if(
                _state->instances,
                [&lookup](const auto& entry)
                {
                    return entry.first.world == lookup.world && entry.first.entity == lookup.entity
                           && entry.first.binding_id == lookup.binding_id;
                });
            return by_binding == _state->instances.end() ? std::weak_ptr<Script> {}
                                                         : by_binding->second.instance;
        }

        const auto iterator = std::ranges::find_if(
            _state->instances,
            [&lookup](const auto& entry)
            {
                return entry.first.world == lookup.world && entry.first.entity == lookup.entity
                       && entry.first.script == lookup.script;
            });
        return iterator == _state->instances.end() ? std::weak_ptr<Script> {}
                                                   : iterator->second.instance;
    }

    void ScriptSystem::consume_script_reloads()
    {
        if (_state->pending_script_reloads.empty())
            return;

        for (auto& entry : _state->instances)
        {
            if (!_state->pending_script_reloads.contains(entry.first.script))
                continue;

            if (entry.second.instance)
                entry.second.instance->on_destroy();

            entry.second.instance = {};
            entry.second.started = false;
        }
        _state->pending_script_reloads.clear();
    }

    void ScriptSystem::on_asset_reloaded(const AssetReloadedEvent& event)
    {
        if (!event.succeeded || !event.affected_asset.id.is_valid())
            return;

        _state->pending_script_reloads.insert(event.affected_asset.id);
    }

    void ScriptSystem::on_plugins_unloading()
    {
        // Runs synchronously while the unloading plugin is still mapped (e.g. a scripts plugin hot-
        // reload). A runtime instance is an object whose vtable and on_destroy live in that module, so
        // destroy every instance and clear the records now — before the library unloads. The next update
        // recreates them against whatever script types are registered after the reload, so the live
        // world (entity state) is preserved; only transient per-instance script state restarts.
        for (auto& entry : _state->instances)
        {
            if (entry.second.instance)
                entry.second.instance->on_destroy();
        }
        _state->instances.clear();

        // The instances are clones of cached Script prototypes whose vtables also live in the unloading
        // module. AssetManager::remove_directory only evicts prototypes under the plugin's resource
        // directory, so a prototype cached from anywhere else would survive with a dangling vtable. Drop
        // every cached script prototype here so none outlives its module; they reload fresh on next use.
        if (auto asset_manager = _asset_manager.lock())
            asset_manager->evict_scripts();
    }
}
