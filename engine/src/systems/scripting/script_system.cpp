#include "tbx/systems/scripting/script_system.h"
#include "tbx/systems/assets/serialization.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/types/assets/world.h"
#include "tbx/types/components/script_container.h"
#include <algorithm>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tbx::internal
{
    struct ScriptInstanceKey
    {
        Uuid world = {};
        Uuid entity = {};
        Uuid script = {};
        Uuid binding_id = {};

        bool operator==(const ScriptInstanceKey& other) const
        {
            return world == other.world && entity == other.entity && script == other.script
                   && binding_id == other.binding_id;
        }
    };

    struct ScriptInstanceKeyHash
    {
        size operator()(const ScriptInstanceKey& key) const
        {
            auto seed = std::hash<uint32>()(key.world.value);
            seed ^= std::hash<uint32>()(key.entity.value) + 0x9e3779b9U + (seed << 6U)
                    + (seed >> 2U);
            seed ^= std::hash<uint32>()(key.script.value) + 0x9e3779b9U + (seed << 6U)
                    + (seed >> 2U);
            seed ^= std::hash<uint32>()(key.binding_id.value) + 0x9e3779b9U + (seed << 6U)
                    + (seed >> 2U);
            return seed;
        }
    };

    struct ScriptInstanceRecord
    {
        std::shared_ptr<Script> script = {};
        bool started = false;
        bool touched = false;
    };
}

namespace tbx
{
    struct ScriptSystem::Impl
    {
        Impl(std::weak_ptr<AssetManager> asset_manager_value, ServiceProvider& services_value)
            : asset_manager(std::move(asset_manager_value))
            , services(services_value)
        {
        }

        std::weak_ptr<AssetManager> asset_manager = {};
        ServiceProvider& services;
        std::unordered_map<
            internal::ScriptInstanceKey,
            internal::ScriptInstanceRecord,
            internal::ScriptInstanceKeyHash>
            instances = {};
    };

    ScriptSystem::ScriptSystem(std::weak_ptr<AssetManager> asset_manager, ServiceProvider& services)
        : _impl(std::make_unique<Impl>(std::move(asset_manager), services))
    {
    }

    ScriptSystem::~ScriptSystem() noexcept = default;

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
        const ScriptBinding& binding)
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

        auto registration = get_asset_type_registration(std::type_index(typeid(*instance)));
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

    static internal::ScriptInstanceKey make_script_instance_key(
        const World& world,
        const Entity& entity,
        const ScriptBinding& binding)
    {
        return internal::ScriptInstanceKey {
            .world = world.id,
            .entity = entity.get_id(),
            .script = binding.script,
            .binding_id = binding.binding_id,
        };
    }

    void ScriptSystem::fixed_update(const DeltaTime& dt)
    {
        auto asset_manager = _impl->asset_manager.lock();
        if (!asset_manager)
            return;

        for (auto& entry : _impl->instances)
            entry.second.touched = false;

        const auto worlds = asset_manager->get_loaded<World>();
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

                        const auto key = make_script_instance_key(*world, entity, binding);
                        auto& record = _impl->instances[key];
                        record.touched = true;
                        if (!record.script)
                            record.script = create_script_instance(*asset_manager, binding);
                        if (!record.script)
                            continue;

                        auto context = ScriptContext(
                            world->id,
                            entity,
                            world.get(),
                            &_impl->services,
                            this);
                        record.script->bind_context(context);
                        if (auto registration =
                                get_asset_type_registration(std::type_index(typeid(*record.script)));
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

    Script* ScriptSystem::try_get_script(const ScriptLookup& lookup)
    {
        if (lookup.binding_id.is_valid())
        {
            auto iterator = _impl->instances.find(
                internal::ScriptInstanceKey {
                    .world = lookup.world,
                    .entity = lookup.entity,
                    .script = lookup.script,
                    .binding_id = lookup.binding_id,
                });
            return iterator == _impl->instances.end() ? nullptr : iterator->second.script.get();
        }

        const auto iterator = std::ranges::find_if(
            _impl->instances,
            [&lookup](const auto& entry)
            {
                return entry.first.world == lookup.world && entry.first.entity == lookup.entity
                       && entry.first.script == lookup.script;
            });
        return iterator == _impl->instances.end() ? nullptr : iterator->second.script.get();
    }

    void ScriptSystem::update(const DeltaTime& dt)
    {
        auto asset_manager = _impl->asset_manager.lock();
        if (!asset_manager)
            return;

        for (auto& entry : _impl->instances)
            entry.second.touched = false;

        const auto worlds = asset_manager->get_loaded<World>();
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

                        const auto key = make_script_instance_key(*world, entity, binding);
                        auto& record = _impl->instances[key];
                        record.touched = true;
                        if (!record.script)
                            record.script = create_script_instance(*asset_manager, binding);
                        if (!record.script)
                            continue;

                        auto context = ScriptContext(
                            world->id,
                            entity,
                            world.get(),
                            &_impl->services,
                            this);
                        record.script->bind_context(context);
                        if (auto registration =
                                get_asset_type_registration(std::type_index(typeid(*record.script)));
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

        auto stale_keys = std::vector<internal::ScriptInstanceKey> {};
        for (const auto& entry : _impl->instances)
        {
            if (!entry.second.touched)
                stale_keys.push_back(entry.first);
        }

        for (const auto& key : stale_keys)
        {
            if (auto iterator = _impl->instances.find(key); iterator != _impl->instances.end())
            {
                if (iterator->second.script)
                    iterator->second.script->on_destroy();
                _impl->instances.erase(iterator);
            }
        }
    }
}
