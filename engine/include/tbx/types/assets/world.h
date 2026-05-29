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
#include <unordered_map>
#include <vector>

namespace tbx
{
    /// @brief Identifies a chunk cell in a world's 3D spatial grid.
    using WorldChunkCoord = IVec3;

    /// @brief
    /// Purpose: References authored chunk assets for one world grid cell.
    [[serializable]];
    struct TBX_API WorldChunkRef
    {
        [[prop]]
        WorldChunkCoord coord = {};

        [[prop]]
        Handle full_chunk = {};
    };

    /// @brief
    /// Purpose: Stores serialized spatial entities for one chunk asset.
    [[serializable]];
    [[version(1U)]];
    struct TBX_API WorldChunk : Asset
    {
        [[prop]]
        WorldChunkCoord coord = {};

        [[prop]]
        std::vector<Entity> entities = {};
    };

    // TODO: Make a WorldManager service that deals with all the actual world logic, owns the world
    // streaming and chunk management and tracks the active world (only one allowed at a time), then
    // make the world asset plain ol data. Also move globals into their own special asset. Globals
    // are loaded first and are kept loaded until the game/app shuts down.
    /// @brief
    /// Purpose: Gameplay-facing entity container with a persistent layer and spatial chunk grid.
    [[serializable]];
    [[version(1U)]];
    [[post_deserialize(rebuild_global_entities)]];
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

        void rebuild_global_entities();
        void update_chunk_membership(float chunk_size = 32.0F);

        bool try_get_chunk(const Uuid& id, WorldChunkCoord& out_coord) const;
        void load_chunk(const WorldChunk& chunk);
        void unload_chunk(const WorldChunkCoord& coord);

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
        std::vector<Entity> globals = {};

        [[prop]]
        std::vector<WorldChunkRef> chunks = {};

      private:
        Entity create_entity(
            const std::string& name,
            const Uuid& parent,
            bool is_persistent,
            float chunk_size);
        void assign_entity_to_chunk(const Entity& entity, float chunk_size);
        bool has_global(const Uuid& id) const;
        void make_persistent(const Uuid& id);
        void remove_global(const Uuid& id);
        void remove_entity_from_chunk_tracking(const Uuid& id);

      private:
        EntityRegistry _registry = {};
        std::unordered_map<WorldChunkCoord, std::vector<Entity>> _loaded_entities_by_chunk = {};
        std::unordered_map<Uuid, WorldChunkCoord> _chunk_by_entity = {};
        std::unordered_map<WorldChunkCoord, std::vector<Uuid>> _entities_by_chunk = {};
    };

}

#include "tbx/types/assets/world.inl"
