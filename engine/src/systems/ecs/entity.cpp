#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/assets/serialization.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/files/json.h"
#include "tbx/systems/plugin_api/plugin_ownership.h"
#include "tbx/systems/plugin_api/plugin_ownership_tracking.h"
#include "tbx/systems/reflection/reflection.h"
#include <shared_mutex>

namespace tbx
{
    using EntityHandle = entt::entity;

    struct SerializedEntityPayload
    {
        Uuid id = {};
        std::string name = {};
        std::string tag = {};
        std::string layer = {};
        Uuid parent = {};
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
            read_typed_serialization_field(json, "tag", payload.tag, std::string {});
            read_typed_serialization_field(json, "layer", payload.layer, std::string {});
            read_typed_serialization_field(json, "parent", payload.parent, Uuid {});
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
            return;
        }

        track_plugin_owned_component_registration(component_type);
    }

    Entity::Entity(const std::string& name, EntityRegistry& registry)
        : Entity(name, Uuid::NONE, registry)
    {
    }

    Entity::Entity(const Uuid& parent, EntityRegistry& registry)
        : Entity("", parent, registry)
    {
    }

    Entity::Entity(const std::string& name, const Uuid& parent, EntityRegistry& registry)
        : _registry(std::ref(registry))
        , _id(registry.add(name, "", "", parent))
    {
    }

    void Entity::destroy()
    {
        if (!_registry.has_value())
            return;

        auto& registry = _registry->get();
        if (!registry.has(_id))
        {
            TBX_ASSERT(false, "Attempted to destroy a stale entity handle.");
            _id = {};
            _registry = std::nullopt;
            return;
        }

        registry.remove(*this);
    }

    Uuid Entity::get_id() const
    {
        if (!_registry.has_value())
            return _id;

        // Callers commonly use `get_id().is_valid()` as a safe validity probe.
        if (!_registry->get().has(_id))
            return {};

        return _id;
    }

    std::string Entity::get_name() const
    {
        if (!_registry.has_value())
            return "";
        auto& registry = _registry->get();
        if (!registry.has(_id))
        {
            TBX_ASSERT(false, "Attempted to read entity name from a stale handle.");
            return "";
        }

        return registry.get_name(_id);
    }

    void Entity::set_name(const std::string& name)
    {
        if (!_registry.has_value())
            return;

        auto& registry = _registry->get();
        if (!registry.has(_id))
        {
            TBX_ASSERT(false, "Attempted to write entity name to a stale handle.");
            return;
        }

        registry.set_name(_id, name);
    }

    std::string Entity::get_tag() const
    {
        if (!_registry.has_value())
            return "";
        auto& registry = _registry->get();
        if (!registry.has(_id))
        {
            TBX_ASSERT(false, "Attempted to read entity tag from a stale handle.");
            return "";
        }

        return registry.get_tag(_id);
    }

    void Entity::set_tag(const std::string& tag)
    {
        if (!_registry.has_value())
            return;

        auto& registry = _registry->get();
        if (!registry.has(_id))
        {
            TBX_ASSERT(false, "Attempted to write entity tag to a stale handle.");
            return;
        }

        registry.set_tag(_id, tag);
    }

    std::string Entity::get_layer() const
    {
        if (!_registry.has_value())
            return "";
        auto& registry = _registry->get();
        if (!registry.has(_id))
        {
            TBX_ASSERT(false, "Attempted to read entity layer from a stale handle.");
            return "";
        }

        return registry.get_layer(_id);
    }

    void Entity::set_layer(const std::string& layer)
    {
        if (!_registry.has_value())
            return;

        auto& registry = _registry->get();
        if (!registry.has(_id))
        {
            TBX_ASSERT(false, "Attempted to write entity layer to a stale handle.");
            return;
        }

        registry.set_layer(_id, layer);
    }

    Uuid Entity::get_parent() const
    {
        if (!_registry.has_value())
            return {};
        auto& registry = _registry->get();
        if (!registry.has(_id))
        {
            TBX_ASSERT(false, "Attempted to read entity parent from a stale handle.");
            return {};
        }

        return registry.get_parent_id(_id);
    }

    void Entity::set_parent(const Uuid& parent)
    {
        if (!_registry.has_value())
            return;

        auto& registry = _registry->get();
        if (!registry.has(_id))
        {
            TBX_ASSERT(false, "Attempted to write entity parent to a stale handle.");
            return;
        }

        registry.set_parent_id(_id, parent);
    }

    bool Entity::try_get_parent_entity(Entity& out_parent) const
    {
        out_parent = Entity {};
        if (!_registry.has_value())
            return false;
        auto& registry = _registry->get();
        if (!registry.has(_id))
        {
            TBX_ASSERT(false, "Attempted to resolve parent from a stale entity handle.");
            return false;
        }

        const Uuid parent_id = registry.get_parent_id(_id);
        if (!parent_id.is_valid())
            return false;

        out_parent = registry.get(parent_id);
        return out_parent.get_id().is_valid();
    }

    Transform Transform::to_world_space(const Entity& entity) const
    {
        auto world_transform = *this;

        auto cursor = entity;
        auto parent = Entity {};
        size_t iteration_count = 0U;
        static constexpr size_t MAX_PARENT_DEPTH = 1024U;
        while (cursor.try_get_parent_entity(parent) && iteration_count < MAX_PARENT_DEPTH)
        {
            if (parent.get_id() == cursor.get_id())
                break;

            if (parent.has_component<Transform>())
            {
                const auto& parent_transform = parent.get_component<Transform>();
                world_transform = compose_world_space_transform(parent_transform, world_transform);
            }

            cursor = parent;
            ++iteration_count;
        }

        return world_transform;
    }

    EntityScope::EntityScope(Entity& source)
        : entity(source)
    {
    }

    EntityScope::~EntityScope() noexcept
    {
        entity.destroy();
    }

    // Grafts a reflected type's editor metadata (category/description/view/readonly/hidden) onto its lean
    // { "type", "value" } serialized form, recursing into nested struct / array-of-struct properties so
    // their fields (e.g. a hidden id) are tagged too. Used by Entity::describe so the editor gets the
    // metadata without bloating persisted data. The component type's own icon is supplied separately,
    // keyed by wire name in the describe response's component_types table.
    static Json enrich_reflected(const TypeReflection& record, const Json& lean)
    {
        if (!lean.is_object())
            return lean;

        auto result = lean;
        for (const auto& property : record.properties)
        {
            const auto node_iterator = result.find(property.name);
            if (node_iterator == result.end() || !node_iterator->is_object())
                continue;

            auto& node = *node_iterator;

            if (const auto value_iterator = node.find(std::string(PROPERTY_VALUE_KEY));
                value_iterator != node.end() && !property.nested_type_name.empty())
            {
                if (const auto* nested = find_type_reflection(property.nested_type_name))
                {
                    if (property.type_token == "object" && value_iterator->is_object())
                    {
                        *value_iterator = enrich_reflected(*nested, *value_iterator);
                    }
                    else if (property.type_token == "array" && value_iterator->is_array())
                    {
                        auto enriched = Json::array();
                        for (const auto& element : *value_iterator)
                            enriched.push_back(enrich_reflected(*nested, element));
                        *value_iterator = std::move(enriched);
                    }
                }
            }

            if (!property.category.empty())
                node["category"] = property.category;
            if (!property.description.empty())
                node["description"] = property.description;
            if (!property.view.empty())
                node["view"] = property.view;
            if (property.readonly)
                node["readonly"] = true;
            if (property.hidden)
                node["hidden"] = true;
        }
        return result;
    }

    std::string Entity::serialize(const Entity& entity)
    {
        auto json = Json::object();
        // Envelope scalars are written as self-describing { "type", "value" } so every value on disk
        // specifies its type. Editor metadata (the id's read-only flag, descriptions) is not persisted;
        // Entity::describe re-adds it from the reflection registry for the editor.
        write_typed_serialization_field(json, "id", entity.get_id());
        write_typed_serialization_field(json, "name", entity.get_name());
        write_typed_serialization_field(json, "tag", entity.get_tag());
        write_typed_serialization_field(json, "layer", entity.get_layer());
        write_typed_serialization_field(json, "parent", entity.get_parent());

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

        json["components"] = components;
        return json.dump();
    }

    std::string Entity::describe(const Entity& entity)
    {
        const auto serialized = serialize(entity);
        auto json = Json::parse(serialized, nullptr, false);
        if (json.is_discarded() || !json.is_object())
            return serialized;

        // The id is engine-assigned, hence read-only; re-add the metadata the lean form drops.
        if (const auto id_iterator = json.find("id");
            id_iterator != json.end() && id_iterator->is_object())
        {
            (*id_iterator)["readonly"] = true;
            (*id_iterator)["description"] = "Stable identity of this entity.";
        }

        if (const auto components_iterator = json.find("components");
            components_iterator != json.end() && components_iterator->is_object())
        {
            for (auto iterator = components_iterator->begin();
                 iterator != components_iterator->end();
                 ++iterator)
            {
                if (const auto* record = find_type_reflection(iterator.key()))
                    *iterator = enrich_reflected(*record, *iterator);
            }
        }

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

    Result Entity::apply_component_json(
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

    Result Entity::with_readable_component(
        const Entity& entity,
        std::string_view component_name,
        const std::function<Result(const EntityComponentTypeRegistration&, const void*)>& action)
    {
        if (!entity._registry.has_value())
            return Result(false, "Entity is not bound to a registry.");

        auto& registry = entity._registry->get();
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
        auto guard = std::shared_lock(registry._mutex);
        if (!registry._registry->valid(handle))
            return Result(false, "Entity handle is no longer valid.");

        const auto* storage = registry._registry->storage(entry->type_id);
        if (storage == nullptr || !storage->contains(handle))
            return Result(
                false,
                std::string("Entity has no '").append(component_name).append("' component."));

        return action(*entry, storage->value(handle));
    }

    Result Entity::with_writable_component(
        const Entity& entity,
        std::string_view component_name,
        const std::function<Result(const EntityComponentTypeRegistration&, void*)>& action)
    {
        if (!entity._registry.has_value())
            return Result(false, "Entity is not bound to a registry.");

        auto& registry = entity._registry->get();
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

        return action(*entry, storage->value(handle));
    }

    Result Entity::get_component_property(
        const Entity& entity,
        std::string_view component_name,
        std::string_view property_name,
        std::string& out_node_json)
    {
        return with_readable_component(
            entity,
            component_name,
            [&](const EntityComponentTypeRegistration& entry, const void* instance) -> Result
            {
                const auto property = reflect(entry.type, instance).property(property_name);
                if (!property.valid())
                    return Result(
                        false,
                        std::string("Unknown property '").append(property_name).append("'."));

                auto node = Json::object();
                node[std::string(PROPERTY_TYPE_KEY)] = std::string(property.type_token());
                node[std::string(PROPERTY_VALUE_KEY)] = property.get();
                out_node_json = node.dump();
                return Result::OK;
            });
    }

    Result Entity::set_component_property(
        const Entity& entity,
        std::string_view component_name,
        std::string_view property_name,
        std::string_view value_json)
    {
        auto value = Json::parse(value_json, nullptr, false);
        if (value.is_discarded())
            return Result(false, "Property value is not valid JSON.");

        return with_writable_component(
            entity,
            component_name,
            [&](const EntityComponentTypeRegistration& entry, void* instance) -> Result
            {
                const auto property = reflect(entry.type, instance).property(property_name);
                if (!property.valid())
                    return Result(
                        false,
                        std::string("Unknown property '").append(property_name).append("'."));

                if (!property.set(value))
                    return Result(
                        false,
                        std::string("Failed to apply property '").append(property_name).append("'."));

                return Result::OK;
            });
    }

    Result Entity::is_component_property_default(
        const Entity& entity,
        std::string_view component_name,
        std::string_view property_name,
        bool& out_is_default)
    {
        return with_readable_component(
            entity,
            component_name,
            [&](const EntityComponentTypeRegistration& entry, const void* instance) -> Result
            {
                const auto property = reflect(entry.type, instance).property(property_name);
                if (!property.valid())
                    return Result(
                        false,
                        std::string("Unknown property '").append(property_name).append("'."));

                out_is_default = property.is_default();
                return Result::OK;
            });
    }

    Result Entity::reset_component_property(
        const Entity& entity,
        std::string_view component_name,
        std::string_view property_name)
    {
        return with_writable_component(
            entity,
            component_name,
            [&](const EntityComponentTypeRegistration& entry, void* instance) -> Result
            {
                const auto property = reflect(entry.type, instance).property(property_name);
                if (!property.valid())
                    return Result(
                        false,
                        std::string("Unknown property '").append(property_name).append("'."));

                const auto* descriptor = property.descriptor();
                if (descriptor == nullptr || !descriptor->has_default)
                    return Result(
                        false,
                        std::string("Property '")
                            .append(property_name)
                            .append("' has no default value."));

                if (!property.set(descriptor->default_value))
                    return Result(
                        false,
                        std::string("Failed to reset property '").append(property_name).append("'."));

                return Result::OK;
            });
    }

    bool Entity::deserialize(std::string_view data, EntityRegistry& registry, Entity& entity)
    {
        auto payload = SerializedEntityPayload();
        if (!read_entity_payload(data, payload))
            return false;

        entity = Entity();
        entity._id =
            registry.add(payload.id, payload.name, payload.tag, payload.layer, payload.parent);
        entity._registry = std::ref(registry);

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
}
