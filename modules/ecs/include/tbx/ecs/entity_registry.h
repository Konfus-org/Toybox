#pragma once
#include "entt/entt.hpp"
#include "tbx/common/uuid.h"
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace tbx
{
    class Entity;

    /// <summary>
    /// Purpose: Owns the ECS registry backend and provides entity lifecycle operations.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns the underlying entt registry instance.
    /// Thread Safety: Not thread-safe; synchronize external concurrent access.
    /// </remarks>
    class TBX_API EntityRegistry
    {
      public:
        EntityRegistry();
        ~EntityRegistry() noexcept;

        bool is_empty() const;
        void clear();

        Uuid add(
            const std::string& name = "",
            const std::string& tag = "",
            const std::string& layer = "",
            const Uuid& parent = Uuid());

        template <typename TComponent>
        void remove(const Uuid& id);
        void remove(Entity& entity);

        template <typename... TComponent>
        std::vector<Entity> get_with();
        std::vector<Entity> get_all();
        Entity get(const Uuid& id);

        template <typename... TComponent>
        void for_each_with(const std::function<void(Entity&)>& callback);
        void for_each(const std::function<void(Entity&)>& callback);

        template <typename TComponent, typename... TArgs>
        TComponent& add(const Uuid& id, TArgs&&... args);

        template <typename... TComponent>
        decltype(auto) get_with(const Uuid& id) const;

        template <typename TComponent>
        bool has(const Uuid& id) const;

      private:
        friend class Entity;

        std::string get_name(const Uuid& id) const;
        void set_name(const Uuid& id, const std::string& name);

        std::string get_tag(const Uuid& id) const;
        void set_tag(const Uuid& id, const std::string& tag);

        Uuid get_parent_id(const Uuid& id) const;
        void set_parent_id(const Uuid& id, const Uuid& parent);

        std::string get_layer(const Uuid& id) const;
        void set_layer(const Uuid& id, const std::string& layer);

        std::unique_ptr<entt::registry> _impl = nullptr;
    };
}

#include "tbx/ecs/entity.h"
#include "tbx/ecs/entity_registry.inl"
