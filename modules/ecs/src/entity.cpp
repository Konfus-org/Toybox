#include "tbx/ecs/entity.h"

namespace tbx
{
    Entity::Entity(EntityRegistry& registry, const Uuid& id, FromRegistryTag)
        : _registry(&registry)
        , _id(id)
    {
    }

    Entity::Entity(const std::string& name, EntityRegistry& registry, const Uuid& parent)
        : _registry(&registry)
        , _id(registry.add(name, "", "", parent))
    {
    }

    Entity::Entity(EntityRegistry& registry, const Uuid& parent)
        : Entity("", registry, parent)
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

    EntityScope::EntityScope(Entity& source)
        : entity(source)
    {
    }

    EntityScope::~EntityScope() noexcept
    {
        entity.destroy();
    }
}
