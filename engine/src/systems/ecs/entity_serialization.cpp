#include "tbx/systems/ecs/entity_serialization.h"
#include "tbx/systems/assets/serialization.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/ecs/registry.h"
#include "tbx/systems/files/json.h"
#include "tbx/systems/plugin_api/runtime_registrations.h"
#include <algorithm>
#include <functional>
#include <mutex>
#include <ranges>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <typeindex>
#include <vector>

namespace tbx
{
    using EntityHandle = entt::entity;

    struct SerializedEntityPayload
    {
        Uuid id = {};
        std::string name = {};
        std::vector<std::string> tags = {};
        std::string layer = {};
        Uuid parent = {};
        int order = 0;
        bool is_enabled = true;
        Json components = {};
    };

    // One plugin's (or the engine core's) component type registrations. Owned by that plugin's
    // RuntimeRegistrations, so dropping the container removes them — and, because the defining plugin is
    // unloading, strips every instance of those component types from all live registries (running
    // each component destructor) while the module is still mapped.
    struct ComponentRegistrations final : RuntimeRegistrationsData
    {
        std::mutex mutex = {};
        std::vector<EntityComponentTypeRegistration> entries = {};

        // Runs only when a plugin's container is dropped on unload (never for the engine core at
        // process exit): the plugin's component types become unavailable, so strip every instance
        // from all live registries while the module is still mapped.
        void on_container_unloading() override
        {
            auto guard = std::lock_guard(mutex);
            for (const auto& entry : entries)
            {
                if (!entry.clear_all)
                    continue;

                for_each_live_entity_registry(
                    [&entry](EntityRegistry& registry)
                    {
                        registry.purge_component(entry.clear_all);
                    });
            }
        }
    };

    static std::vector<EntityComponentTypeRegistration> snapshot_entity_component_type_registrations()
    {
        // Fan out across every container, engine core first, so an engine-owned component wins any
        // name/type collision against a plugin's.
        auto registrations = std::vector<EntityComponentTypeRegistration> {};
        for_each_plugin_runtime(
            [&registrations](RuntimeRegistrations& runtime)
            {
                auto* data = runtime.try_get_data<ComponentRegistrations>();
                if (!data)
                    return;

                auto guard = std::lock_guard(data->mutex);
                registrations.insert(
                    registrations.end(),
                    data->entries.begin(),
                    data->entries.end());
            });
        return registrations;
    }

    static bool read_entity_payload(std::string_view data, SerializedEntityPayload& payload)
    {
        try
        {
            const auto json = JsonParser::parse(data);
            read_serialization_field(json, "id", payload.id, Uuid {});
            read_serialization_field(json, "name", payload.name, std::string {});
            read_serialization_field(json, "tags", payload.tags, std::vector<std::string> {});
            // Back-compat: worlds written before the multi-tag migration carry a single "tag" string;
            // seed it as the entity's one serialized tag when the new "tags" field is absent/empty.
            if (payload.tags.empty())
            {
                auto legacy_tag = std::string {};
                read_serialization_field(json, "tag", legacy_tag, std::string {});
                if (!legacy_tag.empty())
                    payload.tags.push_back(legacy_tag);
            }
            read_serialization_field(json, "layer", payload.layer, std::string {});
            read_serialization_field(json, "parent", payload.parent, Uuid {});
            read_serialization_field(json, "order", payload.order, 0);
            read_serialization_field(json, "is_enabled", payload.is_enabled, true);
            // "components" is a structural keyed collection whose values are already-typed component
            // bodies, so the container itself is not wrapped.
            if (const auto components = json.find("components"); components != json.end())
                payload.components = *components;
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    static EntityHandle to_entity_handle(const Uuid& id)
    {
        if (!id.is_valid())
            return entt::null;

        return static_cast<EntityHandle>(id.value - 1U);
    }

    std::vector<EntityComponentTypeRegistration> get_entity_component_type_registrations()
    {
        return snapshot_entity_component_type_registrations();
    }

    void register_entity_component_type_entry(
        RuntimeRegistrations& owner,
        EntityComponentTypeRegistration entry)
    {
        if (entry.type == std::type_index(typeid(void)) || entry.type_id == entt::id_type())
            return;

        // Whichever container already owns this component type keeps it. The engine core is visited
        // first, so a plugin can neither shadow an engine component nor come to "own" (and later
        // purge, on unload) a type it did not define — it only enriches an entry in its OWN
        // container.
        auto owned_by_other = false;
        for_each_plugin_runtime(
            [&](RuntimeRegistrations& runtime)
            {
                if (&runtime == &owner)
                    return;

                auto* data = runtime.try_get_data<ComponentRegistrations>();
                if (!data)
                    return;

                auto guard = std::lock_guard(data->mutex);
                if (std::ranges::any_of(
                        data->entries,
                        [&entry](const EntityComponentTypeRegistration& registered)
                        {
                            return registered.type == entry.type;
                        }))
                    owned_by_other = true;
            });

        if (owned_by_other)
            return;

        auto& data = owner.get_data<ComponentRegistrations>();
        auto guard = std::lock_guard(data.mutex);
        const auto existing = std::ranges::find_if(
            data.entries,
            [&entry](const EntityComponentTypeRegistration& registered)
            {
                return registered.type == entry.type;
            });
        if (existing == data.entries.end())
        {
            data.entries.push_back(std::move(entry));
            return;
        }

        // Same container already carries this type: enrich in place, order-independently.
        if (!entry.name.empty())
            existing->name = std::move(entry.name);
        if (!entry.type_name.empty())
            existing->type_name = std::move(entry.type_name);
        if (entry.write_value)
            existing->write_value = std::move(entry.write_value);
        if (entry.read_value)
            existing->read_value = std::move(entry.read_value);
        if (entry.copy_value)
            existing->copy_value = std::move(entry.copy_value);
        if (entry.clear_all)
            existing->clear_all = std::move(entry.clear_all);
    }

    std::string Entity::serialize(const Entity& entity, bool include_defaults)
    {
        // Persisted entities omit properties equal to their default (include_defaults == false). The
        // scope governs the whole serialize call graph: each component's generated serialize honors it.
        const auto omit_scope = OmitDefaultFieldsScope(!include_defaults);

        auto json = Json::object();
        // The id is always written (identity must never be omitted); the rest are plain values.
        write_serialization_field(json, "id", entity.get_id());
        write_serialization_field(json, "name", entity.get_name());
        write_serialization_field(json, "tags", entity.get_persistent_tags());
        write_serialization_field(json, "layer", entity.get_layer());
        write_serialization_field(json, "parent", entity.get_parent());
        write_serialization_field(json, "order", entity.get_order());
        write_serialization_field(json, "is_enabled", entity.is_enabled());

        auto components = Json::object();
        if (entity._registry.has_value())
        {
            auto& registry = entity._registry->get();
            if (registry.has(entity._id))
            {
                const auto entity_name = entity.get_name();
                const auto entries = get_entity_component_type_registrations();
                const auto handle = to_entity_handle(entity._id);
                auto guard = std::shared_lock(registry._mutex);
                for (const auto& entry : entries)
                {
                    const auto* storage = registry._registry->storage(entry.type_id);
                    if (storage == nullptr || !storage->contains(handle))
                        continue;

                    if (!entry.write_value || entry.name.empty())
                    {
                        TBX_TRACE_WARNING(
                            "Entity '{}' skipped unserializable component '{}'.",
                            entity_name,
                            entry.type_name);
                        continue;
                    }

                    const auto* value = storage->value(handle);
                    if (value == nullptr)
                        continue;

                    components[entry.name] = JsonParser::parse(entry.write_value(value));
                }
            }
        }

        json["components"] = std::move(components);
        return json.dump();
    }

    bool Entity::deserialize(std::string_view data, Entity& entity)
    {
        auto owned_registry = std::make_shared<EntityRegistry>();
        auto rebound_entity = Entity();
        if (!deserialize(data, *owned_registry, rebound_entity))
            return false;

        rebound_entity._owned_registry = std::move(owned_registry);
        entity = std::move(rebound_entity);
        return true;
    }

    bool Entity::deserialize(std::string_view data, EntityRegistry& registry, Entity& entity)
    {
        auto payload = SerializedEntityPayload();
        if (!read_entity_payload(data, payload))
            return false;

        entity = Entity();
        entity._id = registry.add(payload.id, payload.name, payload.layer, payload.parent);
        entity._registry = std::ref(registry);
        entity.set_order(payload.order);
        entity.set_enabled(payload.is_enabled);
        // Persisted tags are all serialized (runtime tags are never written); restore them as such.
        for (const auto& tag : payload.tags)
            entity.add_tag(tag, /*serialized*/ true);

        if (payload.components.is_null())
            return true;

        const auto entries = get_entity_component_type_registrations();
        const auto handle = to_entity_handle(entity._id);
        for (const auto& [key, component_json] : payload.components.items())
        {
            const auto entry = std::ranges::find_if(
                entries,
                [&key](const EntityComponentTypeRegistration& registered)
                {
                    return registered.name == key;
                });
            if (entry == entries.end() || !entry->read_value)
            {
                TBX_TRACE_WARNING(
                    "Entity '{}' skipped unsupported serialized component '{}'.",
                    entity.get_name(),
                    key);
                continue;
            }

            auto registry_guard = std::unique_lock(registry._mutex);
            if (!registry._registry->valid(handle))
                return false;

            if (!entry->read_value(component_json.dump(), *registry._registry, handle))
                return false;
        }

        return true;
    }

    std::string EntityRegistry::serialize(const EntityRegistry& registry)
    {
        auto entities = Json::array();
        for (const auto& entity : registry.get_all())
        {
            // Transient entities (the editor bridge's injected view cameras) are never written.
            if (!entity.get_id().is_valid() || !entity.is_serialized())
                continue;

            auto record = Json::parse(Entity::serialize(entity), nullptr, false);
            if (!record.is_discarded())
                entities.push_back(std::move(record));
        }
        return entities.dump();
    }

    bool EntityRegistry::deserialize(std::string_view data, EntityRegistry& registry)
    {
        auto json = Json::parse(data, nullptr, false);
        if (json.is_discarded())
            return false;
        if (!json.is_array())
            return true;

        for (const auto& record : json)
        {
            auto entity = Entity();
            Entity::deserialize(record.dump(), registry, entity);
        }
        return true;
    }

    Result serialize_component(
        const Entity& entity,
        std::string_view component_name,
        std::string& out_json)
    {
        if (!entity._registry.has_value())
            return Result(false, "Entity is not bound to a registry.");

        auto& registry = entity._registry->get();
        const auto entries = get_entity_component_type_registrations();
        const auto entry = std::ranges::find_if(
            entries,
            [&component_name](const EntityComponentTypeRegistration& candidate)
            { return candidate.name == component_name; });
        if (entry == entries.end() || !entry->write_value)
            return Result(
                false,
                std::string("Unknown component '").append(component_name).append("'."));

        const auto handle = to_entity_handle(entity._id);
        auto guard = std::shared_lock(registry._mutex);
        if (!registry._registry->valid(handle))
            return Result(false, "Entity handle is no longer valid.");

        const auto* storage = registry._registry->storage(entry->type_id);
        if (storage == nullptr || !storage->contains(handle))
            return Result(
                false,
                std::string("Entity has no '").append(component_name).append("' component."));

        out_json = entry->write_value(storage->value(handle));
        return Result::OK;
    }

    Result apply_component(
        const Entity& entity,
        std::string_view component_name,
        std::string_view value_json)
    {
        if (!entity._registry.has_value())
            return Result(false, "Entity is not bound to a registry.");

        auto& registry = entity._registry->get();
        if (!registry.has(entity._id))
            return Result(false, "Entity no longer exists.");

        const auto entries = get_entity_component_type_registrations();
        const auto entry = std::ranges::find_if(
            entries,
            [&component_name](const EntityComponentTypeRegistration& candidate)
            { return candidate.name == component_name; });
        if (entry == entries.end() || !entry->read_value)
            return Result(
                false,
                std::string("Unknown component '").append(component_name).append("'."));

        const auto handle = to_entity_handle(entity._id);
        auto guard = std::unique_lock(registry._mutex);
        if (!registry._registry->valid(handle))
            return Result(false, "Entity handle is no longer valid.");

        if (!entry->read_value(value_json, *registry._registry, handle))
            return Result(
                false,
                std::string("Failed to apply component '").append(component_name).append("'."));

        return Result::OK;
    }

    Result add_default_component(const Entity& entity, std::string_view component_name)
    {
        if (!entity._registry.has_value())
            return Result(false, "Entity is not bound to a registry.");

        auto& registry = entity._registry->get();
        if (!registry.has(entity._id))
            return Result(false, "Entity no longer exists.");

        const auto entries = get_entity_component_type_registrations();
        const auto entry = std::ranges::find_if(
            entries,
            [&component_name](const EntityComponentTypeRegistration& candidate)
            { return candidate.name == component_name; });
        if (entry == entries.end() || !entry->read_value)
            return Result(
                false,
                std::string("Unknown component '").append(component_name).append("'."));

        const auto handle = to_entity_handle(entity._id);
        auto guard = std::unique_lock(registry._mutex);
        if (!registry._registry->valid(handle))
            return Result(false, "Entity handle is no longer valid.");

        if (const auto* storage = registry._registry->storage(entry->type_id);
            storage != nullptr && storage->contains(handle))
            return Result(
                false,
                std::string("Entity already has a '").append(component_name).append("' component."));

        // Emplace the component at its defaults: reading an empty object applies no fields, so the
        // generated deserialize leaves every field at its in-source default.
        if (!entry->read_value("{}", *registry._registry, handle))
            return Result(
                false,
                std::string("Failed to add component '").append(component_name).append("'."));

        return Result::OK;
    }

    Result remove_component(const Entity& entity, std::string_view component_name)
    {
        if (!entity._registry.has_value())
            return Result(false, "Entity is not bound to a registry.");

        auto& registry = entity._registry->get();
        if (!registry.has(entity._id))
            return Result(false, "Entity no longer exists.");

        const auto entries = get_entity_component_type_registrations();
        const auto entry = std::ranges::find_if(
            entries,
            [&component_name](const EntityComponentTypeRegistration& candidate)
            { return candidate.name == component_name; });
        if (entry == entries.end())
            return Result(
                false,
                std::string("Unknown component '").append(component_name).append("'."));

        const auto handle = to_entity_handle(entity._id);
        auto guard = std::unique_lock(registry._mutex);
        if (!registry._registry->valid(handle))
            return Result(false, "Entity handle is no longer valid.");

        auto* storage = registry._registry->storage(entry->type_id);
        if (storage == nullptr || !storage->contains(handle))
            return Result(
                false,
                std::string("Entity has no '").append(component_name).append("' component."));

        // Erasing from the type's storage runs the component's destructor and frees the slot.
        storage->remove(handle);
        return Result::OK;
    }

    Result serialize_component_property(
        const Entity& entity,
        std::string_view component_name,
        std::string_view property_name,
        std::string& out_value_json)
    {
        // Read a property by serializing the whole component — every field is present on this path
        // (no default omission) — and lifting out its bare value.
        std::string component_json;
        if (const auto read = serialize_component(entity, component_name, component_json); !read)
            return read;

        const auto component = Json::parse(component_json, nullptr, false);
        const auto field = component.is_object() ? component.find(std::string(property_name))
                                                 : component.end();
        if (field == component.end())
            return Result(
                false,
                std::string("Unknown property '").append(property_name).append("'."));

        out_value_json = field->dump();
        return Result::OK;
    }

    Result apply_component_property(
        const Entity& entity,
        std::string_view component_name,
        std::string_view property_name,
        std::string_view value_json)
    {
        auto value = Json::parse(value_json, nullptr, false);
        if (value.is_discarded())
            return Result(false, "Property value is not valid JSON.");

        // Edit by round-trip: serialize the whole component, overwrite the one field's value, then
        // deserialize it back via apply_component. Going through the component's own
        // serialize/deserialize reaches private [[serialize]] fields without naming them and needs no
        // per-property accessors.
        std::string component_json;
        if (const auto read = serialize_component(entity, component_name, component_json); !read)
            return read;

        auto component = Json::parse(component_json, nullptr, false);
        if (!component.is_object())
            return Result(false, "Component did not serialize to an object.");

        const auto field = component.find(std::string(property_name));
        if (field == component.end())
            return Result(
                false,
                std::string("Unknown property '").append(property_name).append("'."));

        *field = std::move(value);
        return apply_component(entity, component_name, component.dump());
    }
}
