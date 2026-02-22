#include "tbx/ecs/entity.h"
#include <cstddef>

namespace tbx
{
    static Vec3 multiply_components(const Vec3& left, const Vec3& right)
    {
        return Vec3(left.x * right.x, left.y * right.y, left.z * right.z);
    }

    static Transform compose_world_space_transform(
        const Transform& parent_transform,
        const Transform& local_transform)
    {
        auto world_transform = Transform {};
        world_transform.scale = multiply_components(parent_transform.scale, local_transform.scale);
        world_transform.rotation = normalize(parent_transform.rotation * local_transform.rotation);
        world_transform.position =
            parent_transform.position
            + (parent_transform.rotation
               * multiply_components(parent_transform.scale, local_transform.position));
        return world_transform;
    }

    Entity::Entity(const std::string& name, EntityRegistry* registry)
        : Entity(name, Uuid::NONE, registry)
    {
    }

    Entity::Entity(const std::string& name, EntityRegistry& registry)
        : Entity(name, Uuid::NONE, registry)
    {
    }

    Entity::Entity(const std::string& name, const Uuid& parent, EntityRegistry* registry)
        : _registry(registry)
        , _id(registry->add(name, "", "", parent))
    {
    }

    Entity::Entity(const Uuid& parent, EntityRegistry* registry)
        : Entity("", parent, registry)
    {
    }

    Entity::Entity(const Uuid& parent, EntityRegistry& registry)
        : Entity("", parent, registry)
    {
    }

    Entity::Entity(const std::string& name, const Uuid& parent, EntityRegistry& registry)
        : _registry(&registry)
        , _id(registry.add(name, "", "", parent))
    {
    }

    void Entity::destroy()
    {
        if (_registry == nullptr)
            return;

        _registry->remove(*this);
    }

    Uuid Entity::get_id() const
    {
        if (_registry == nullptr)
            return {};

        return _id;
    }

    std::string Entity::get_name() const
    {
        if (_registry == nullptr)
            return "";

        return _registry->get_name(_id);
    }

    void Entity::set_name(const std::string& name)
    {
        if (_registry == nullptr)
            return;

        _registry->set_name(_id, name);
    }

    std::string Entity::get_tag() const
    {
        if (_registry == nullptr)
            return "";

        return _registry->get_tag(_id);
    }

    void Entity::set_tag(const std::string& tag)
    {
        if (_registry == nullptr)
            return;

        _registry->set_tag(_id, tag);
    }

    std::string Entity::get_layer() const
    {
        if (_registry == nullptr)
            return "";

        return _registry->get_layer(_id);
    }

    void Entity::set_layer(const std::string& layer)
    {
        if (_registry == nullptr)
            return;

        _registry->set_layer(_id, layer);
    }

    Uuid Entity::get_parent() const
    {
        if (_registry == nullptr)
            return {};

        return _registry->get_parent_id(_id);
    }

    void Entity::set_parent(const Uuid& parent)
    {
        if (_registry == nullptr)
            return;

        _registry->set_parent_id(_id, parent);
    }

    bool Entity::try_get_parent_entity(Entity& out_parent) const
    {
        out_parent = Entity {};
        if (_registry == nullptr)
            return false;

        const Uuid parent_id = _registry->get_parent_id(_id);
        if (!parent_id.is_valid())
            return false;

        out_parent = _registry->get(parent_id);
        return out_parent.get_id().is_valid();
    }

    std::string to_string(const Entity& entity)
    {
        auto idValue = std::to_string(entity.get_id().value);
        auto nameValue = entity.get_name();
        auto tagValue = entity.get_tag();
        auto layerValue = entity.get_layer();
        auto parentValue = std::to_string(entity.get_parent().value);

        std::string value = {};
        value.reserve(
            32U + idValue.size() + nameValue.size() + tagValue.size() + layerValue.size()
            + parentValue.size());

        value += "Entity{";
        value += "id=";
        value += idValue;
        value += ", name='";
        value += nameValue;
        value += "'";
        value += ", tag='";
        value += tagValue;
        value += "'";
        value += ", layer='";
        value += layerValue;
        value += "'";
        value += ", parent=";
        value += parentValue;
        value += "}";
        return value;
    }

    Transform get_world_space_transform(const Entity& entity)
    {
        auto world_transform = Transform {};
        if (entity.has_component<Transform>())
            world_transform = entity.get_component<Transform>();

        auto cursor = entity;
        auto parent = Entity {};
        size_t iteration_count = 0U;
        static constexpr size_t max_parent_depth = 1024U;
        while (cursor.try_get_parent_entity(parent) && iteration_count < max_parent_depth)
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
}
