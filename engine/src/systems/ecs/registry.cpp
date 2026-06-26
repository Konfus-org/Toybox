#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/ecs/registry.h"
#include "tbx/systems/ecs/tag_id.h"
#include "tbx/systems/plugin_api/plugin_ownership_tracking.h"
#include <algorithm>
#include <vector>

namespace tbx
{
    using EntityHandle = entt::entity;

    struct EntityNameComponent
    {
        std::string value = "";
    };

    // UE-style gameplay tags. An entity carries any number of hierarchical, dot-delimited tags
    // (e.g. "editor.selected"), each held in exactly one of two lists: serialized tags persist with
    // the world; runtime tags are transient editor/gameplay state that is never written. has_tag
    // matches hierarchically (a query "editor" matches the tag "editor.selected").
    struct EntityTagsComponent
    {
        // Serialized tags persist with the world, so they stay strings (written/read as-is). Runtime
        // tags are transient editor/gameplay state, so they are stored as interned TagIds — matching is
        // an int compare and there is no per-entity string storage. Their names round-trip through the
        // tag-id registry for get_tags.
        std::vector<std::string> serialized = {};
        std::vector<TagId> runtime = {};
    };

    struct EntityLayerComponent
    {
        std::string value = "";
    };

    struct EntityParentComponent
    {
        Uuid value = {};
    };

    // Explicit sibling order, so the editor can present and persist a user-chosen arrangement rather than
    // entt's insertion order. Lower sorts first among entities sharing a parent.
    struct EntityOrderComponent
    {
        int value = 0;
    };

    // Wholesale enable flag for the entity. A disabled entity is skipped by the typed component queries
    // (get_with / first_with / for_each_with), so every runtime system that gathers through them —
    // rendering, physics, scripting — passes it over, while the editor (get_all / get / serialize) still
    // sees it so it can be listed and re-enabled. Defaults enabled.
    struct EntityEnabledComponent
    {
        bool value = true;
    };

    static EntityHandle to_entity_handle(const Uuid& id)
    {
        if (!id.is_valid())
            return entt::null;

        return static_cast<EntityHandle>(id.value - 1U);
    }

    static Uuid to_entity_id(EntityHandle handle)
    {
        const auto handle_value = static_cast<uint32>(entt::to_integral(handle));
        return Uuid(handle_value + 1U);
    }

    // Hierarchical gameplay-tag match: an exact match, or the query naming a parent segment of the
    // tag ("editor" matches "editor.selected", but not "editorial").
    static bool tag_matches(const std::string& tag, const std::string& query)
    {
        if (tag == query)
            return true;
        return tag.size() > query.size() && tag.starts_with(query) && tag[query.size()] == '.';
    }

    static bool list_contains(const std::vector<std::string>& list, const std::string& value)
    {
        for (const auto& entry : list)
            if (entry == value)
                return true;
        return false;
    }

    template <typename TComponent, typename TValue>
    static void set_component_value(entt::registry& registry, const Uuid& id, TValue&& value)
    {
        auto entityHandle = to_entity_handle(id);
        if (!registry.valid(entityHandle))
            return;

        if (!registry.all_of<TComponent>(entityHandle))
        {
            registry.emplace<TComponent>(
                entityHandle,
                TComponent {.value = std::forward<TValue>(value)});
            return;
        }

        registry.get<TComponent>(entityHandle).value = std::forward<TValue>(value);
    }

    template <typename TComponent, typename TValue>
    static TValue get_component_value(const entt::registry& registry, const Uuid& id)
    {
        auto entityHandle = to_entity_handle(id);
        if (!registry.valid(entityHandle))
            return {};

        if (!registry.all_of<TComponent>(entityHandle))
            return {};

        return registry.get<TComponent>(entityHandle).value;
    }

    EntityRegistry::EntityRegistry()
        : _registry(std::make_unique<entt::registry>())
    {
    }

    EntityRegistry::EntityRegistry(const EntityRegistry& other)
        : _registry(std::make_unique<entt::registry>())
    {
        for (const auto& entity : other.get_all())
            absorb(entity);
    }

    EntityRegistry& EntityRegistry::operator=(const EntityRegistry& other)
    {
        if (this == &other)
            return *this;

        clear();
        for (const auto& entity : other.get_all())
            absorb(entity);
        return *this;
    }

    EntityRegistry::~EntityRegistry() noexcept = default;

    bool EntityRegistry::is_empty() const
    {
        auto guard = std::shared_lock(_mutex);
        auto view = _registry->view<EntityNameComponent>();
        return view.empty();
    }

    void EntityRegistry::clear()
    {
        auto guard = std::unique_lock(_mutex);
        _registry = std::make_unique<entt::registry>();
    }

    bool EntityRegistry::has(const Uuid& id) const
    {
        auto guard = std::shared_lock(_mutex);
        return _registry->valid(to_entity_handle(id));
    }

    Uuid EntityRegistry::add(
        const std::string& name,
        const std::string& layer,
        const Uuid& parent)
    {
        auto guard = std::unique_lock(_mutex);
        const EntityHandle handle = _registry->create();
        const auto id = to_entity_id(handle);

        auto resolvedName = name;
        if (resolvedName.empty())
            resolvedName = std::format("{}", id);

        _registry->emplace<EntityNameComponent>(
            handle,
            EntityNameComponent {.value = resolvedName});
        _registry->emplace<EntityLayerComponent>(handle, EntityLayerComponent {.value = layer});
        _registry->emplace<EntityParentComponent>(handle, EntityParentComponent {.value = parent});
        _registry->emplace<EntityOrderComponent>(handle, EntityOrderComponent {});
        _registry->emplace<EntityEnabledComponent>(handle, EntityEnabledComponent {});

        track_plugin_owned_entity(id);

        return id;
    }

    Uuid EntityRegistry::add(
        const Uuid& id,
        const std::string& name,
        const std::string& layer,
        const Uuid& parent)
    {
        if (!id.is_valid())
            return add(name, layer, parent);

        auto guard = std::unique_lock(_mutex);
        const EntityHandle handle = to_entity_handle(id);
        if (!_registry->valid(handle))
        {
            const auto created_handle = _registry->create(handle);
            TBX_ASSERT(created_handle == handle, "Failed to create entity handle '{}'.", id);
        }

        auto resolvedName = name;
        if (resolvedName.empty())
            resolvedName = std::format("{}", id);

        _registry->emplace_or_replace<EntityNameComponent>(
            handle,
            EntityNameComponent {.value = resolvedName});
        _registry->emplace_or_replace<EntityLayerComponent>(
            handle,
            EntityLayerComponent {.value = layer});
        _registry->emplace_or_replace<EntityParentComponent>(
            handle,
            EntityParentComponent {.value = parent});
        // Order defaults to 0 here; a persisted value is applied afterwards via set_order_value (the
        // add overloads carry only the four identity fields).
        if (!_registry->all_of<EntityOrderComponent>(handle))
            _registry->emplace<EntityOrderComponent>(handle, EntityOrderComponent {});
        // Enabled defaults true here; a persisted value is applied afterwards via set_enabled, mirroring
        // order. Preserved if the entity already exists so a plain re-add doesn't silently re-enable it.
        if (!_registry->all_of<EntityEnabledComponent>(handle))
            _registry->emplace<EntityEnabledComponent>(handle, EntityEnabledComponent {});

        track_plugin_owned_entity(id);

        return id;
    }

    void EntityRegistry::remove(Entity& entity)
    {
        auto guard = std::unique_lock(_mutex);
        if (!entity._registry.has_value() || &entity._registry->get() != this)
            return;

        auto handle = to_entity_handle(entity._id);
        if (!_registry->valid(handle))
        {
            TBX_ASSERT(false, "Attempted to remove a stale entity handle from the registry.");
            return;
        }

        _registry->destroy(handle);
        entity._id = {};
        entity._registry = std::nullopt;
    }

    Entity EntityRegistry::get(const Uuid& id) const
    {
        auto guard = std::shared_lock(_mutex);
        if (!_registry->valid(to_entity_handle(id)))
            return {};

        auto entity = Entity {};
        entity._id = id;
        entity._registry = std::ref(const_cast<EntityRegistry&>(*this));
        return entity;
    }

    std::vector<Entity> EntityRegistry::get_all() const
    {
        auto guard = std::shared_lock(_mutex);
        std::vector<Entity> entities = {};
        auto view = _registry->view<EntityNameComponent>();

        for (const auto entityHandle : view)
        {
            auto id = to_entity_id(entityHandle);
            auto entity = Entity {};
            entity._id = id;
            entity._registry = std::ref(const_cast<EntityRegistry&>(*this));
            entities.push_back(entity);
        }

        return entities;
    }

    void EntityRegistry::for_each(const std::function<void(Entity&)>& callback)
    {
        if (!callback)
            return;

        auto ids = std::vector<Uuid> {};
        {
            auto guard = std::shared_lock(_mutex);
            auto view = _registry->view<EntityNameComponent>();
            for (const auto entityHandle : view)
                ids.push_back(to_entity_id(entityHandle));
        }

        for (const auto& id : ids)
        {
            auto entity = get(id);
            callback(entity);
        }
    }

    std::string EntityRegistry::get_name(const Uuid& id) const
    {
        auto guard = std::shared_lock(_mutex);
        return get_component_value<EntityNameComponent, std::string>(*_registry, id);
    }

    void EntityRegistry::set_name(const Uuid& id, const std::string& name)
    {
        auto guard = std::unique_lock(_mutex);
        set_component_value<EntityNameComponent>(*_registry, id, name);
    }

    void EntityRegistry::add_tag(const Uuid& id, const std::string& name, bool serialized)
    {
        if (name.empty())
            return;

        auto guard = std::unique_lock(_mutex);
        const auto handle = to_entity_handle(id);
        if (!_registry->valid(handle))
            return;

        if (!_registry->all_of<EntityTagsComponent>(handle))
            _registry->emplace<EntityTagsComponent>(handle);
        auto& tags = _registry->get<EntityTagsComponent>(handle);

        // A tag lives in exactly one list; re-adding with the other persistence promotes/demotes it.
        // Serialized tags are strings (persisted); runtime tags are interned ids.
        const TagId id_for_runtime = intern_tag(name);
        if (serialized)
        {
            std::erase(tags.runtime, id_for_runtime);
            if (!list_contains(tags.serialized, name))
                tags.serialized.push_back(name);
        }
        else
        {
            std::erase(tags.serialized, name);
            if (std::ranges::find(tags.runtime, id_for_runtime) == tags.runtime.end())
                tags.runtime.push_back(id_for_runtime);
        }
    }

    void EntityRegistry::remove_tag(const Uuid& id, const std::string& name)
    {
        auto guard = std::unique_lock(_mutex);
        const auto handle = to_entity_handle(id);
        if (!_registry->valid(handle) || !_registry->all_of<EntityTagsComponent>(handle))
            return;

        auto& tags = _registry->get<EntityTagsComponent>(handle);
        std::erase(tags.serialized, name);
        std::erase(tags.runtime, intern_tag(name));
    }

    bool EntityRegistry::has_tag(const Uuid& id, const std::string& query) const
    {
        auto guard = std::shared_lock(_mutex);
        const auto handle = to_entity_handle(id);
        if (!_registry->valid(handle) || !_registry->all_of<EntityTagsComponent>(handle))
            return false;

        const auto& tags = _registry->get<EntityTagsComponent>(handle);
        for (const auto& tag : tags.serialized)
            if (tag_matches(tag, query))
                return true;
        // Runtime tags match on id, hierarchically (the query is the ancestor): "editor" matches a
        // runtime "editor.selected".
        const TagId query_id = intern_tag(query);
        for (const TagId tag : tags.runtime)
            if (tag_is_ancestor(query_id, tag))
                return true;
        return false;
    }

    std::vector<std::string> EntityRegistry::get_tags(const Uuid& id) const
    {
        auto guard = std::shared_lock(_mutex);
        const auto handle = to_entity_handle(id);
        std::vector<std::string> all = {};
        if (!_registry->valid(handle) || !_registry->all_of<EntityTagsComponent>(handle))
            return all;

        const auto& tags = _registry->get<EntityTagsComponent>(handle);
        all.insert(all.end(), tags.serialized.begin(), tags.serialized.end());
        for (const TagId tag : tags.runtime)
            all.push_back(tag_name(tag));
        return all;
    }

    std::vector<std::string> EntityRegistry::get_persistent_tags(const Uuid& id) const
    {
        auto guard = std::shared_lock(_mutex);
        const auto handle = to_entity_handle(id);
        if (!_registry->valid(handle) || !_registry->all_of<EntityTagsComponent>(handle))
            return {};
        return _registry->get<EntityTagsComponent>(handle).serialized;
    }

    std::vector<std::string> EntityRegistry::get_runtime_tags(const Uuid& id) const
    {
        auto guard = std::shared_lock(_mutex);
        const auto handle = to_entity_handle(id);
        if (!_registry->valid(handle) || !_registry->all_of<EntityTagsComponent>(handle))
            return {};
        const auto& runtime = _registry->get<EntityTagsComponent>(handle).runtime;
        std::vector<std::string> names = {};
        names.reserve(runtime.size());
        for (const TagId tag : runtime)
            names.push_back(tag_name(tag));
        return names;
    }

    Uuid EntityRegistry::get_parent_id(const Uuid& id) const
    {
        auto guard = std::shared_lock(_mutex);
        return get_component_value<EntityParentComponent, Uuid>(*_registry, id);
    }

    void EntityRegistry::set_parent_id(const Uuid& id, const Uuid& parent)
    {
        auto guard = std::unique_lock(_mutex);
        set_component_value<EntityParentComponent>(*_registry, id, parent);
    }

    int EntityRegistry::get_order_value(const Uuid& id) const
    {
        auto guard = std::shared_lock(_mutex);
        return get_component_value<EntityOrderComponent, int>(*_registry, id);
    }

    void EntityRegistry::set_order_value(const Uuid& id, int order)
    {
        auto guard = std::unique_lock(_mutex);
        set_component_value<EntityOrderComponent>(*_registry, id, order);
    }

    bool EntityRegistry::get_enabled(const Uuid& id) const
    {
        auto guard = std::shared_lock(_mutex);
        const auto handle = to_entity_handle(id);
        // Absent flag means enabled: every entity gets one on add, but treating absence as enabled keeps
        // any entity created outside the add path (and the query filter) from vanishing.
        if (!_registry->valid(handle) || !_registry->all_of<EntityEnabledComponent>(handle))
            return true;

        return _registry->get<EntityEnabledComponent>(handle).value;
    }

    void EntityRegistry::set_enabled(const Uuid& id, bool enabled)
    {
        auto guard = std::unique_lock(_mutex);
        set_component_value<EntityEnabledComponent>(*_registry, id, enabled);
    }

    bool EntityRegistry::is_handle_enabled(entt::entity handle) const
    {
        // Mirrors get_enabled but takes a resolved handle and does not lock: callers hold _mutex.
        // Absent flag means enabled, matching every other entity-enabled read path.
        if (!_registry->valid(handle) || !_registry->all_of<EntityEnabledComponent>(handle))
            return true;

        return _registry->get<EntityEnabledComponent>(handle).value;
    }

    bool EntityRegistry::is_handle_effectively_enabled(entt::entity handle) const
    {
        // An entity is effectively enabled only when it AND every ancestor is enabled, so disabling a parent
        // turns its whole subtree off for the runtime queries without touching each child's own stored flag.
        // Walks up the parent chain under the caller-held lock, depth-capped and self-parent guarded like
        // Transform::to_world_space so a malformed cycle can't spin forever.
        static constexpr int MAX_PARENT_DEPTH = 1024;
        auto cursor = handle;
        for (auto depth = 0; depth < MAX_PARENT_DEPTH; ++depth)
        {
            if (!is_handle_enabled(cursor))
                return false;
            if (!_registry->valid(cursor) || !_registry->all_of<EntityParentComponent>(cursor))
                return true;

            const auto parent = _registry->get<EntityParentComponent>(cursor).value;
            if (!parent.is_valid())
                return true;

            const auto parent_handle = to_entity_handle(parent);
            if (parent_handle == cursor || !_registry->valid(parent_handle))
                return true;

            cursor = parent_handle;
        }

        return true;
    }

    bool EntityRegistry::get_effective_enabled(const Uuid& id) const
    {
        auto guard = std::shared_lock(_mutex);
        return is_handle_effectively_enabled(to_entity_handle(id));
    }

    std::string EntityRegistry::get_layer(const Uuid& id) const
    {
        auto guard = std::shared_lock(_mutex);
        return get_component_value<EntityLayerComponent, std::string>(*_registry, id);
    }

    void EntityRegistry::set_layer(const Uuid& id, const std::string& layer)
    {
        auto guard = std::unique_lock(_mutex);
        set_component_value<EntityLayerComponent>(*_registry, id, layer);
    }

    void EntityRegistry::absorb(const Entity& source)
    {
        const auto id = source.get_id();
        if (!id.is_valid() || !source._registry.has_value())
            return;

        auto& source_registry = source._registry->get();
        if (&source_registry == this)
            return;

        // Recreate identity metadata in this registry, then copy each registered component by value.
        add(id, source.get_name(), source.get_layer(), source.get_parent());
        set_order_value(id, source.get_order());
        set_enabled(id, source.is_enabled());
        // Copy only the persistent tags. Runtime tags (selection, editor state) are intentionally NOT
        // carried across a registry copy: they are transient per-registry state, so a play-mode
        // snapshot or a saved chunk never inherits e.g. "editor.selected" (which would otherwise outline
        // entities in the played/loaded world). The editor re-applies them to the live world as needed.
        for (const auto& tag : source.get_persistent_tags())
            add_tag(id, tag, /*serialized*/ true);

        const auto entries = get_entity_component_type_registrations();
        const auto handle = to_entity_handle(id);
        auto source_guard = std::shared_lock(source_registry._mutex);
        auto guard = std::unique_lock(_mutex);
        for (const auto& entry : entries)
        {
            const auto* storage = source_registry._registry->storage(entry.type_id);
            if (storage == nullptr || !storage->contains(handle) || !entry.copy_value)
                continue;

            entry.copy_value(storage->value(handle), *_registry, handle);
        }
    }
}
