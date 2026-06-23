#include "tbx/systems/ecs/entity_serialization.h"
#include "tbx/systems/assets/serialization.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/ecs/registry.h"
#include "tbx/systems/files/json.h"
#include "tbx/systems/plugin_api/plugin_ownership.h"
#include "tbx/systems/plugin_api/plugin_ownership_tracking.h"
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

    class EntityComponentRegistrationStore final
    {
      public:
        static EntityComponentRegistrationStore& get_instance()
        {
            static EntityComponentRegistrationStore store = {};
            return store;
        }

      public:
        EntityComponentRegistrationStore(const EntityComponentRegistrationStore&) = delete;
        EntityComponentRegistrationStore& operator=(const EntityComponentRegistrationStore&) =
            delete;
        EntityComponentRegistrationStore(EntityComponentRegistrationStore&&) = delete;
        EntityComponentRegistrationStore& operator=(EntityComponentRegistrationStore&&) = delete;

      public:
        std::mutex& mutex()
        {
            return _mutex;
        }

        std::vector<EntityComponentTypeRegistration>& registrations()
        {
            return _registrations;
        }

      private:
        EntityComponentRegistrationStore() = default;
        ~EntityComponentRegistrationStore() noexcept = default;

      private:
        std::mutex _mutex = {};
        std::vector<EntityComponentTypeRegistration> _registrations = {};
    };

    static std::vector<EntityComponentTypeRegistration> snapshot_entity_component_type_registrations()
    {
        auto& store = EntityComponentRegistrationStore::get_instance();
        auto guard = std::lock_guard(store.mutex());
        return store.registrations();
    }

    static bool read_entity_payload(std::string_view data, SerializedEntityPayload& payload)
    {
        try
        {
            const auto json = JsonParser::parse(data);
            // Envelope scalars use the self-describing { "type", "value" } wrapper.
            read_typed_serialization_field(json, "id", payload.id, Uuid {});
            read_typed_serialization_field(json, "name", payload.name, std::string {});
            read_typed_serialization_field(
                json, "tags", payload.tags, std::vector<std::string> {});
            // Back-compat: worlds written before the multi-tag migration carry a single "tag" string;
            // seed it as the entity's one serialized tag when the new "tags" field is absent/empty.
            if (payload.tags.empty())
            {
                auto legacy_tag = std::string {};
                read_typed_serialization_field(json, "tag", legacy_tag, std::string {});
                if (!legacy_tag.empty())
                    payload.tags.push_back(legacy_tag);
            }
            read_typed_serialization_field(json, "layer", payload.layer, std::string {});
            read_typed_serialization_field(json, "parent", payload.parent, Uuid {});
            read_typed_serialization_field(json, "order", payload.order, 0);
            read_typed_serialization_field(json, "is_enabled", payload.is_enabled, true);
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

    void unregister_entity_component_type_entry(std::type_index component_type)
    {
        if (component_type == std::type_index(typeid(void)))
            return;

        auto& store = EntityComponentRegistrationStore::get_instance();
        auto guard = std::lock_guard(store.mutex());
        auto& registrations = store.registrations();
        const auto iterator = std::ranges::find_if(
            registrations,
            [component_type](const EntityComponentTypeRegistration& registration)
            {
                return registration.type == component_type;
            });
        if (iterator != registrations.end())
            registrations.erase(iterator);
    }

    void register_entity_component_type_entry(EntityComponentTypeRegistration entry)
    {
        if (entry.type == std::type_index(typeid(void)) || entry.type_id == entt::id_type())
            return;
        const auto component_type = entry.type;

        // Component serializers live process-wide so plugin unload can remove plugin-owned
        // component registrations across worlds.
        auto& store = EntityComponentRegistrationStore::get_instance();
        auto guard = std::lock_guard(store.mutex());
        auto& registrations = store.registrations();
        const auto existing = std::ranges::find_if(
            registrations,
            [&entry](const EntityComponentTypeRegistration& registered)
            {
                return registered.type == entry.type;
            });
        if (existing == registrations.end())
        {
            registrations.push_back(std::move(entry));
        }
        else
        {
            // Prevent plugin code from replacing engine-owned serializers for existing components.
            if (has_active_plugin_id())
                return;

            if (!entry.name.empty())
                existing->name = std::move(entry.name);
            if (!entry.type_name.empty())
                existing->type_name = std::move(entry.type_name);
            if (entry.write_value)
                existing->write_value = std::move(entry.write_value);
            if (entry.read_value)
                existing->read_value = std::move(entry.read_value);
            if (!entry.icon.empty())
                existing->icon = std::move(entry.icon);
            if (!entry.icon_color.empty())
                existing->icon_color = std::move(entry.icon_color);
            if (entry.describe)
                existing->describe = std::move(entry.describe);
            return;
        }

        track_plugin_owned_component_registration(component_type);
    }

    // Reshapes the entity's id envelope node into the same { "attributes", "value", "is_default" } form
    // the components carry under attribute serialization. The components themselves are already attributed
    // by their own generated serialize (run under AttributeSerializationScope), so only the engine-managed
    // id — which is written lean by the envelope writer — needs reshaping here. The id is engine-assigned,
    // hence read-only and never "default".
    static void enrich_entity_id(Json& json)
    {
        if (!json.is_object())
            return;

        const auto id_iterator = json.find("id");
        if (id_iterator == json.end() || !id_iterator->is_object())
            return;

        auto& id_node = *id_iterator;
        auto attributes = Json::object();
        if (const auto type_iterator = id_node.find(std::string(PROPERTY_TYPE_KEY));
            type_iterator != id_node.end())
            attributes[std::string(PROPERTY_TYPE_KEY)] = *type_iterator;
        attributes["readonly"] = true;
        attributes["description"] = "Stable identity of this entity.";

        auto rebuilt = Json::object();
        if (const auto value_iterator = id_node.find(std::string(PROPERTY_VALUE_KEY));
            value_iterator != id_node.end())
            rebuilt[std::string(PROPERTY_VALUE_KEY)] = std::move(*value_iterator);
        rebuilt[std::string(PROPERTY_ATTRIBUTES_KEY)] = std::move(attributes);
        rebuilt[std::string(PROPERTY_IS_DEFAULT_KEY)] = false;
        id_node = std::move(rebuilt);
    }

    std::string Entity::serialize(
        const Entity& entity,
        bool include_defaults,
        bool include_attributes)
    {
        // Persisted entities omit properties equal to their default (include_defaults == false);
        // attribute enrichment needs every field, so it forces the full set. Both scopes govern the whole
        // serialize call graph: each component's generated serialize honors them, emitting attribute-rich
        // nodes when include_attributes is on.
        const auto write_all = include_defaults || include_attributes;
        const auto omit_scope = OmitDefaultFieldsScope(!write_all);
        const auto attribute_scope = AttributeSerializationScope(include_attributes);

        auto json = Json::object();
        // Envelope scalars are written as self-describing { "type", "value" } so every value on disk
        // specifies its type. The id is always written (identity must never be omitted).
        write_typed_serialization_field(json, "id", entity.get_id());
        write_typed_serialization_field(json, "name", entity.get_name());
        write_typed_serialization_field(json, "tags", entity.get_persistent_tags());
        write_typed_serialization_field(json, "layer", entity.get_layer());
        write_typed_serialization_field(json, "parent", entity.get_parent());
        write_typed_serialization_field(json, "order", entity.get_order());
        write_typed_serialization_field(json, "is_enabled", entity.is_enabled());

        auto components = Json::object();
        // The order components are visited (their registration order) before the JSON object alphabetizes
        // its keys; emitted as component_order on the attribute path so the editor lists components in this
        // natural order rather than alphabetically.
        auto component_order = Json::array();
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
                    component_order.push_back(entry.name);
                }
            }
        }

        json["components"] = components;
        if (include_attributes)
            json["component_order"] = std::move(component_order);

        // Components are already attribute-enriched by their own serialize under the scope above; only the
        // engine-managed id envelope needs reshaping into the same attributed form.
        if (include_attributes)
            enrich_entity_id(json);

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
            if (!entity.get_id().is_valid())
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

    Result serialize_component_property(
        const Entity& entity,
        std::string_view component_name,
        std::string_view property_name,
        std::string& out_node_json)
    {
        // Read a property by serializing the whole component — every field is present on this path
        // (no default omission) — and lifting out its { "type", "value" } node.
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

        out_node_json = field->dump();
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
        // serialize/deserialize reaches private [[prop]] fields without naming them and needs no
        // per-property accessors.
        std::string component_json;
        if (const auto read = serialize_component(entity, component_name, component_json); !read)
            return read;

        auto component = Json::parse(component_json, nullptr, false);
        if (!component.is_object())
            return Result(false, "Component did not serialize to an object.");

        const auto field = component.find(std::string(property_name));
        if (field == component.end() || !field->is_object())
            return Result(
                false,
                std::string("Unknown property '").append(property_name).append("'."));

        (*field)[std::string(PROPERTY_VALUE_KEY)] = std::move(value);
        return apply_component(entity, component_name, component.dump());
    }

    // Resolves a component property's default value from the type's describe(false) thunk — a lean,
    // all-fields serialization of a default-constructed instance carried on the component registration.
    // Returns false when the component/property is unknown or the type has no captured default (e.g. it is
    // not default-constructible). This replaces the old type-reflection registry's captured defaults.
    static bool find_component_property_default(
        std::string_view component_name,
        std::string_view property_name,
        Json& out_value)
    {
        const auto entries = get_entity_component_type_registrations();
        const auto entry = std::ranges::find_if(
            entries,
            [&component_name](const EntityComponentTypeRegistration& candidate)
            { return candidate.name == component_name; });
        if (entry == entries.end() || !entry->describe)
            return false;

        const auto schema = Json::parse(entry->describe(false), nullptr, false);
        const auto field = schema.is_object() ? schema.find(std::string(property_name)) : schema.end();
        if (field == schema.end() || !field->is_object())
            return false;

        const auto value = field->find(std::string(PROPERTY_VALUE_KEY));
        if (value == field->end())
            return false;

        out_value = *value;
        return true;
    }

    Result is_component_property_default(
        const Entity& entity,
        std::string_view component_name,
        std::string_view property_name,
        bool& out_is_default)
    {
        // Read the live property value (this also validates the component/property exist).
        std::string node_json;
        if (const auto read =
                serialize_component_property(entity, component_name, property_name, node_json);
            !read)
            return read;

        auto default_value = Json();
        if (!find_component_property_default(component_name, property_name, default_value))
        {
            // No captured default to compare against.
            out_is_default = false;
            return Result::OK;
        }

        const auto node = Json::parse(node_json, nullptr, false);
        const auto value = node.is_object() ? node.find(std::string(PROPERTY_VALUE_KEY))
                                            : node.end();
        out_is_default = value != node.end() && *value == default_value;
        return Result::OK;
    }

    Result reset_component_property(
        const Entity& entity,
        std::string_view component_name,
        std::string_view property_name)
    {
        auto default_value = Json();
        if (!find_component_property_default(component_name, property_name, default_value))
            return Result(
                false,
                std::string("Property '").append(property_name).append("' has no default value."));

        // Reset is just a set to the captured default value.
        return apply_component_property(
            entity,
            component_name,
            property_name,
            default_value.dump());
    }
}
