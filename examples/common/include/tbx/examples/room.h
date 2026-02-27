#pragma once
#include "tbx/ecs/entity.h"

namespace examples_common
{
    /// <summary>Settings used to create a reusable boxed room for rendering and physics
    /// examples.</summary> <remarks> Purpose: Defines transform and collider options for floor and
    /// wall geometry generation. Ownership: Value type copied by Room during creation. Thread
    /// Safety: Safe to copy across threads; room creation itself is main-thread ECS work.
    /// </remarks>
    struct RoomSettings final
    {
        tbx::Vec3 center = tbx::Vec3(0.0F, 0.0F, 0.0F);
        bool include_colliders = true;
    };

    /// <summary>Creates and owns room entities shared by example scenes.</summary>
    /// <remarks>
    /// Purpose: Encapsulates creation/destruction of a floor and surrounding walls.
    /// Ownership: Stores non-owning entity handles; ECS registry owns entity lifetime.
    /// Thread Safety: Not thread-safe; call from the main thread that owns ECS mutation.
    /// </remarks>
    class Room final
    {
      public:
        /// <summary>Builds a room into the target registry using the provided settings.</summary>
        /// <remarks>
        /// Purpose: Spawns floor and wall entities and optional collider components.
        /// Ownership: Does not own the registry; caller must keep registry alive for room lifetime.
        /// Thread Safety: Main-thread only because it mutates ECS state.
        /// </remarks>
        void create(tbx::EntityRegistry& entity_registry, const RoomSettings& settings);

        /// <summary>Destroys any currently tracked room entities.</summary>
        /// <remarks>
        /// Purpose: Cleans up spawned room entities when an example detaches.
        /// Ownership: Only invalidates non-owning handles managed by this class.
        /// Thread Safety: Main-thread only because it mutates ECS state.
        /// </remarks>
        void destroy();

      private:
        tbx::Entity _ground = {};
        tbx::Entity _front_wall = {};
        tbx::Entity _left_wall = {};
        tbx::Entity _right_wall = {};
        tbx::Entity _back_wall = {};
    };
}
