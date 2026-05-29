#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/files/json.h"
#include "tbx/systems/plugin_api/plugin_loader.h"
#include "tbx/systems/plugin_api/plugin_ownership.h"
#include "tbx/systems/plugin_api/plugin_ownership_tracker.h"
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

    static std::vector<EntityComponentTypeRegistration>& entity_component_type_registrations()
    {
        static auto registrations = std::vector<EntityComponentTypeRegistration> {};
        return registrations;
    }

    static std::mutex& entity_component_type_registration_mutex()
    {
        static auto mutex = std::mutex();
        return mutex;
    }

    static std::vector<EntityComponentTypeRegistration> snapshot_entity_component_type_registrations()
    {
        auto guard = std::lock_guard(entity_component_type_registration_mutex());
        return entity_component_type_registrations();
    }

    static bool read_entity_payload(std::string_view data, SerializedEntityPayload& payload)
    {
        try
        {
            const auto json = JsonParser::parse(data);
            JsonParser::try_get(json, "id", payload.id);
            JsonParser::try_get(json, "name", payload.name);
            JsonParser::try_get(json, "tag", payload.tag);
            JsonParser::try_get(json, "layer", payload.layer);
            JsonParser::try_get(json, "parent", payload.parent);
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

        auto guard = std::lock_guard(entity_component_type_registration_mutex());
        auto& registrations = entity_component_type_registrations();
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
        if (is_plugin_meta_query_active())
            return;

        if (entry.type == std::type_index(typeid(void)) || entry.type_id == entt::id_type())
            return;
        const auto component_type = entry.type;

        auto guard = std::lock_guard(entity_component_type_registration_mutex());
        auto& registrations = entity_component_type_registrations();
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

        if (!has_active_plugin_id())
            return;

        if (auto tracker = lock_plugin_ownership_tracker())
            tracker->track_component_registration(get_active_plugin_id(), component_type);
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

    Transform get_world_space_transform(const Entity& entity)
    {
        auto world_transform = Transform {};
        if (entity.has_component<Transform>())
            world_transform = entity.get_component<Transform>();

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

    std::string Entity::serialize(const Entity& entity)
    {
        auto json = Json::object();
        ::tbx::serialize(json["id"], entity.get_id());
        json["name"] = entity.get_name();
        json["tag"] = entity.get_tag();
        json["layer"] = entity.get_layer();
        ::tbx::serialize(json["parent"], entity.get_parent());

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

    bool Entity::deserialize(
        std::string_view data,
        EntityRegistry& registry,
        Entity& entity)
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
