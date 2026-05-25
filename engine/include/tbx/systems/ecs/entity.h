#pragma once
#include "tbx/types/component.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/uuid.h"
#include <concepts>
#include <format>
#include <functional>
#include <optional>
#include <string>
#include <utility>

namespace tbx
{
    class EntityRegistry;

    /// @brief
    /// Purpose: Represents a lightweight handle to an entity owned by an EntityRegistry.
    /// @details
    /// Ownership: Does not own the registry; caller ensures registry lifetime exceeds this
    /// instance. Thread Safety: Not thread-safe; synchronize external concurrent access.
    class TBX_API Entity
    {
      public:
        Entity() = default;
        Entity(const std::string& name, EntityRegistry& registry);
        Entity(const std::string& name, const Uuid& parent, EntityRegistry& registry);
        Entity(const Uuid& parent, EntityRegistry& registry);

        void destroy();

        Uuid get_id() const;

        std::string get_name() const;
        void set_name(const std::string& name);

        std::string get_tag() const;
        void set_tag(const std::string& tag);

        std::string get_layer() const;
        void set_layer(const std::string& layer);

        Uuid get_parent() const;
        void set_parent(const Uuid& parent);

        /// @brief
        /// Purpose: Resolves and returns this entity's parent entity handle.
        /// @details
        /// Ownership: Writes a non-owning entity handle into out_parent when available.
        /// Thread Safety: Not thread-safe; synchronize external concurrent access.
        bool try_get_parent_entity(Entity& out_parent) const;

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

      private:
        friend class EntityRegistry;

        std::optional<std::reference_wrapper<EntityRegistry>> _registry = std::nullopt;
        Uuid _id = {};
    };

    /// @brief
    /// Purpose: Resolves an entity transform in world space by composing parent local transforms.
    /// @details
    /// Ownership: Returns an owned Transform value snapshot.
    /// Thread Safety: Not thread-safe; synchronize external concurrent access. Notes: Entity
    /// Transform components are authored and stored in local space.
    TBX_API Transform get_world_space_transform(const Entity& entity);

    /// @brief
    /// Purpose: RAII wrapper that destroys the wrapped entity on scope exit.
    /// @details
    /// Ownership: Owns the contained Entity value only; does not own registry state.
    /// Thread Safety: Not thread-safe; synchronize external concurrent access.
    class TBX_API EntityScope
    {
      public:
        EntityScope(Entity& source);
        ~EntityScope() noexcept;

        Entity entity;
    };
}

template <>
struct std::formatter<tbx::Entity>
{
    constexpr auto parse(std::format_parse_context& ctx)
    {
        return _formatter.parse(ctx);
    }

    template <typename TFormatContext>
    auto format(const tbx::Entity& entity, TFormatContext& ctx) const
    {
        return _formatter.format(
            std::format(
                "Entity{{id={}, name='{}', tag='{}', layer='{}', parent={}}}",
                entity.get_id().value,
                entity.get_name(),
                entity.get_tag(),
                entity.get_layer(),
                entity.get_parent().value),
            ctx);
    }

    std::formatter<std::string> _formatter;
};

#include "tbx/systems/ecs/entity.inl"
