#pragma once
#include "entt/entt.hpp"
#include "tbx/systems/ecs/entity.generated.h"
#include "tbx/types/components/component.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/uuid.h"
#include <concepts>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Represents a lightweight handle to an entity owned by an EntityRegistry.
    /// @details
    /// Ownership: Does not own the registry; caller ensures registry lifetime exceeds this
    /// instance. Thread Safety: Not thread-safe; synchronize external concurrent access.
    [[serializable]];
    [[custom_serialization(serialize, deserialize)]];
    [[printable(
        "Entity{{id={}, name='{}', layer='{}', parent={}}}",
        get_id().value,
        get_name(),
        get_layer(),
        get_parent().value)]];
    class TBX_API Entity
    {
      public:
        Entity() = default;
        Entity(const std::string& name, class EntityRegistry& registry);
        Entity(const std::string& name, const Uuid& parent, class EntityRegistry& registry);
        Entity(const Uuid& parent, class EntityRegistry& registry);

      public:
        void destroy();

        Uuid get_id() const;

        std::string get_name() const;
        void set_name(const std::string& name);

        // Gameplay tags (UE-style, hierarchical dot-names e.g. "editor.selected"). A tag is either
        // serialized (persists with the world) or runtime (transient editor/gameplay state, never
        // written). has_tag matches hierarchically — query "editor" matches the tag
        // "editor.selected".
        void add_tag(const std::string& name, bool serialized = true);
        void remove_tag(const std::string& name);
        bool has_tag(const std::string& query) const;
        std::vector<std::string> get_tags() const;
        std::vector<std::string> get_persistent_tags() const;
        std::vector<std::string> get_runtime_tags() const;

        std::string get_layer() const;
        void set_layer(const std::string& layer);

        Uuid get_parent() const;
        void set_parent(const Uuid& parent);
        bool try_get_parent_entity(Entity& out_parent) const;

        // Explicit ordering among siblings (entities sharing a parent); lower sorts first.
        // Editor-driven and persisted, so a user-arranged world reloads in the same order.
        int get_order() const;
        void set_order(int order);

        // Wholesale enable flag. A disabled entity is skipped by every typed component query
        // (get_with / first_with / for_each_with), so rendering, physics and scripting all pass it
        // over — it stays in the world (and the editor) but is inert until re-enabled. Persisted;
        // defaults true.
        bool is_enabled() const;
        void set_enabled(bool enabled);

        template <typename TComponent>
            requires std::derived_from<TComponent, Component>
        TComponent& add_component(const TComponent& component);

        template <typename TComponent, typename... TArgs>
            requires std::derived_from<TComponent, Component>
        TComponent& add_component(TArgs&&... args);

        template <typename TComponent>
            requires std::derived_from<TComponent, Component>
        void remove_component();

        template <typename... TComponent>
            requires(std::derived_from<TComponent, Component> && ...)
        decltype(auto) get_components() const;

        template <typename TComponent>
            requires std::derived_from<TComponent, Component>
        TComponent& get_component() const;

        template <typename TComponent>
            requires std::derived_from<TComponent, Component>
        bool has_component() const;

      public:
        /// @brief Serializes the entity to the self-describing { "type", "value" } form. By default
        /// (include_defaults == false) properties equal to their default are omitted to keep
        /// persisted files small; the reader reconstructs them from the type's defaults.
        ///
        /// Pass include_attributes == true to additionally enrich every property node with its
        /// reflection metadata ({ "attributes": { type, nested, order, choices }, "value",
        /// "is_default" }); this also forces every field to be written.
        /// Persisted files use the lean form (both flags false); tooling that needs the full
        /// reflected view passes both true.
        static std::string serialize(
            const Entity& entity,
            bool include_defaults = false,
            bool include_attributes = false);
        static bool deserialize(std::string_view data, Entity& entity);
        static bool deserialize(
            std::string_view data,
            class EntityRegistry& registry,
            Entity& entity);

      private:
        friend class EntityRegistry;
        // The component-level serialization primitives reach into the registry's component storage,
        // which is otherwise private to the ECS core (see entity_serialization.h).
        friend TBX_API Result serialize_component(
            const Entity& entity,
            std::string_view component_name,
            std::string& out_json);
        friend TBX_API Result apply_component(
            const Entity& entity,
            std::string_view component_name,
            std::string_view value_json);
        friend TBX_API Result add_default_component(
            const Entity& entity,
            std::string_view component_name);
        friend TBX_API Result remove_component(
            const Entity& entity,
            std::string_view component_name);
        // Binds an entity-reference field to a target id during serialization (see below).
        friend TBX_API void bind_reference(Entity& entity, const Uuid& id);

        std::shared_ptr<class EntityRegistry> _owned_registry = nullptr;
        std::optional<std::reference_wrapper<class EntityRegistry>> _registry = std::nullopt;
        Uuid _id = {};
    };

    // Entity-reference serialization. A tbx::Entity used as a component/script *field* is a
    // reference to another entity, so it serializes as just the target id (token "entity"), not the
    // whole entity — the generic serializer finds these ADL hooks and special-cases the field. The
    // inspector then shows an entity picker. A bound reference holds only the id (no registry); the
    // game resolves it via the world.
    TBX_API Uuid reference_id(const Entity& entity);
    TBX_API void bind_reference(Entity& entity, const Uuid& id);

    /// @brief
    /// Purpose: RAII wrapper that destroys the wrapped entity on scope exit.
    /// @details
    /// Ownership: Owns the contained Entity value only; does not own registry state.
    /// Thread Safety: Not thread-safe; synchronize external concurrent access.
    class TBX_API EntityScope
    {
      public:
        explicit EntityScope(Entity& source);
        ~EntityScope() noexcept;

        Entity entity;
    };
}

// Entity's template methods operate through the registry, so pull in the full EntityRegistry
// definition (and the template bodies) after Entity is complete. registry.h includes this header
// first, so #pragma once makes this a no-op when registry.h is the entry point.
#include "tbx/systems/ecs/registry.h"
