#pragma once
#include "tbx/systems/assets/serialization.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/ecs/registry.h"
#include "tbx/tbx_api.h"
#include "tbx/types/assets/asset.h"
#include "tbx/types/assets/world.generated.h"
#include "tbx/types/handle.h"
#include "tbx/types/uuid.h"
#include "tbx/types/vectors.h"
#include <concepts>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Stores serialized spatial entities for one chunk asset.
    [[serializable]];
    [[version(1U)]];
    struct TBX_API WorldChunk : Asset
    {
        [[prop]]
        IVec3 coord = {};

        [[prop]]
        std::vector<Entity> entities = {};
    };

    /// @brief
    /// Purpose: Stores global entities that stay resident for the full application lifetime.
    [[serializable]];
    [[version(1U)]];
    struct TBX_API WorldGlobals : Asset
    {
        [[prop]]
        std::vector<Entity> entities = {};
    };

    /// @brief
    /// Purpose: Gameplay-facing entity container backed by a plain world asset description.
    [[serializable]];
    [[version(1U)]];
    class TBX_API World : public Asset
    {
      public:
        World();
        ~World() noexcept;

      public:
        World(const World&) = delete;
        World& operator=(const World&) = delete;
        World(World&&) noexcept = delete;
        World& operator=(World&&) noexcept = delete;

      public:
        Entity create_entity(const std::string& name = "");
        Entity create_entity(const std::string& name, const Uuid& parent);
        Entity create_global_entity(const std::string& name = "");

        void destroy(Entity& entity);
        void clear_runtime_entities();

        void add_entities(const std::vector<Entity>& entities);
        void load_globals(const WorldGlobals& globals);
        void remove_entities(const std::vector<Uuid>& ids);

        bool is_global(const Uuid& id) const;

        Entity find_by_name(std::string_view name) const;
        Entity find_by_tag(std::string_view tag) const;

        Entity get(const Uuid& id) const;
        std::vector<Entity> get_all() const;

        bool has(const Uuid& id) const;

        template <typename TComponent>
            requires std::derived_from<TComponent, Component>
        bool has(const Uuid& id) const;

        template <typename... TComponent>
            requires(std::derived_from<TComponent, Component> && ...)
        std::vector<Entity> get_with() const;

        template <typename... TComponent>
            requires(std::derived_from<TComponent, Component> && ...)
        Entity first_with() const;

        template <typename... TComponent>
            requires(std::derived_from<TComponent, Component> && ...)
        void for_each_with(const std::function<void(Entity&)>& callback);

      public:
        [[prop]]
        Handle globals = {};

        [[prop]]
        std::vector<Handle> chunks = {};

      private:
        bool has_global(const Uuid& id) const;
        void make_persistent(const Uuid& id);
        void remove_global(const Uuid& id);

      private:
        EntityRegistry _registry = {};
        std::unordered_set<Uuid> _global_entities = {};
    };
}

#include "tbx/types/assets/world.inl"
