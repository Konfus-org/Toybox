#pragma once
#include "tbx/systems/assets/serialization.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/ecs/registry.h"
#include "tbx/tbx_api.h"
#include "tbx/types/assets/asset.h"
#include "tbx/types/components/world_simulation_state.h"
#include "tbx/types/handle.h"
#include "tbx/types/typedefs.h"
#include "tbx/types/uuid.h"
#include "tbx/types/vectors.h"
#include <concepts>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tbx
{
    /// @brief Identifies a chunk cell in a world's 3D spatial grid.
    using WorldChunkCoord = IVec3;

    /// @brief
    /// Purpose: References authored chunk assets for one world grid cell.
    [[tbx::serializable]];
    [[tbx::prop(coord, full_chunk, low_lod_chunk, simulation_chunk)]];
    struct TBX_API WorldChunkRef
    {
        WorldChunkCoord coord = {};
        Handle full_chunk = {};
        Handle low_lod_chunk = {};
        Handle simulation_chunk = {};
    };

    /// @brief
    /// Purpose: Describes the visual detail currently loaded for a chunk.
    [[tbx::serializable]];
    enum class WorldChunkLod
    {
        FULL [[tbx::name("full")]],
        LOW [[tbx::name("low")]]
    };

    /// @brief
    /// Purpose: Stores serialized spatial entities for one chunk asset.
    [[tbx::serializable]];
    [[tbx::version(1U)]];
    struct TBX_API WorldChunk : Asset
    {
        [[tbx::prop]]
        WorldChunkCoord coord = {};

        [[tbx::prop]]
        WorldChunkLod lod = WorldChunkLod::FULL;

        [[tbx::prop]]
        std::vector<Entity> entities = {};
    };

    /// @brief
    /// Purpose: Gameplay-facing entity container with a persistent layer and spatial chunk grid.
    [[tbx::serializable]];
    [[tbx::version(1U)]];
    [[tbx::prop(
        chunk_size,
        full_visual_radius_chunks,
        low_visual_radius_chunks,
        simulation_radius_chunks,
        reduced_simulation_radius_chunks,
        unload_radius_chunks,
        persistent_entities,
        chunks)]];
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
        Entity create_persistent_entity(const std::string& name = "");
        Entity create_spatial_entity(const std::string& name = "");
        Entity create_spatial_entity(const std::string& name, const Uuid& parent);

        void destroy(Entity& entity);
        void clear_runtime_entities();
        void rebuild_persistent_entities();
        void update_chunk_membership();

        bool has(const Uuid& id) const;
        bool is_persistent(const Uuid& id) const;
        bool try_get_chunk(const Uuid& id, WorldChunkCoord& out_coord) const;

        Entity find_by_id(const Uuid& id) const;
        Entity find_by_name(std::string_view name) const;
        Entity find_by_tag(std::string_view tag) const;
        Entity get(const Uuid& id) const;
        std::vector<Entity> get_all() const;

        void load_chunk(const WorldChunk& chunk, WorldSimulationMode simulation_mode);
        void unload_chunk(const WorldChunkCoord& coord);
        void set_chunk_simulation_mode(
            const WorldChunkCoord& coord,
            WorldSimulationMode simulation_mode);

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
        // TODO: extract setting props into a WorldSettings struct that we put on the AppSettings
        float chunk_size = 32.0F;
        uint32 full_visual_radius_chunks = 2U;
        uint32 low_visual_radius_chunks = 6U;
        uint32 simulation_radius_chunks = 2U;
        uint32 reduced_simulation_radius_chunks = 4U;
        uint32 unload_radius_chunks = 8U;

        std::vector<Entity> persistent_entities = {};
        std::vector<WorldChunkRef> chunks = {};

      private:
        Entity create_entity(const std::string& name, const Uuid& parent, bool is_persistent);
        void assign_entity_to_chunk(const Entity& entity);
        void make_persistent(const Uuid& id);
        void remove_entity_from_chunk_tracking(const Uuid& id);

      private:
        EntityRegistry _registry = {};
        std::unordered_map<WorldChunkCoord, std::vector<Entity>> _loaded_entities_by_chunk = {};
        std::unordered_set<Uuid> _persistent_entities = {};
        std::unordered_map<Uuid, WorldChunkCoord> _chunk_by_entity = {};
        std::unordered_map<WorldChunkCoord, std::vector<Uuid>> _entities_by_chunk = {};
    };

    inline void tbx_after_deserialize(World& world)
    {
        world.rebuild_persistent_entities();
    }

}

#include "tbx/types/assets/world.generated.h"
#include "tbx/types/assets/world.inl"
