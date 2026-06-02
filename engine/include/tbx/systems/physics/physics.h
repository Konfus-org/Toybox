#pragma once
#include "tbx/interfaces/physics_backend.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/assets/reload_queue.h"
#include "tbx/systems/ecs/world/manager.h"
#include "tbx/systems/physics/settings.h"
#include "tbx/types/assets/world.h"
#include "tbx/types/quaternions.h"
#include "tbx/types/raycast.h"

namespace tbx
{
    /// @brief
    /// Purpose: Tracks backend physics resources and transform state for one ECS entity.
    /// @details
    /// Ownership: Stores non-owning backend handles; Physics owns record lifetime.
    /// Thread Safety: Not thread-safe; owned by the main-thread Physics service.
    struct PhysicsEntityRecord
    {
        PhysicsColliderHandle collider = {};
        PhysicsRigidbodyHandle rigidbody = {};
        Vec3 last_position = Vec3(0.0F, 0.0F, 0.0F);
        Quat last_rotation = Quat(1.0F, 0.0F, 0.0F, 0.0F);
        Vec3 last_scale = Vec3(1.0F, 1.0F, 1.0F);
        bool has_last_transform = false;
        bool is_physics_driven = false;
        bool is_trigger_only = false;
    };

    /// @brief
    /// Purpose: Application-owned physics service that synchronizes ECS components with the
    /// registered physics backend.
    /// @details
    /// Ownership: Borrows application services and owns entity-to-backend resource state.
    /// Thread Safety: Not thread-safe; call from the application main thread.
    class TBX_API Physics final
    {
      public:
        Physics(
            std::weak_ptr<IPhysicsBackend> backend,
            std::weak_ptr<AssetManager> asset_manager,
            std::weak_ptr<WorldManager> world_manager,
            const PhysicsSettings& settings);
        Physics(
            std::weak_ptr<IPhysicsBackend> backend,
            std::weak_ptr<AssetManager> asset_manager,
            std::weak_ptr<WorldManager> world_manager,
            std::weak_ptr<AssetReloadQueue> reload_queue,
            const PhysicsSettings& settings);
        ~Physics() noexcept;

      public:
        Physics(const Physics&) = delete;
        Physics& operator=(const Physics&) = delete;
        Physics(Physics&&) noexcept = delete;
        Physics& operator=(Physics&&) noexcept = delete;

      public:
        RaycastResult raycast(const RaycastQuery& raycast_query) const;
        void update(const DeltaTime& dt, const PhysicsSettings& settings);

      private:
        void clear_resources();
        static PhysicsBackendSettings get_backend_settings(const PhysicsSettings& settings);
        void process_trigger_colliders(World& world);
        void sync_entities_to_backend(World& world, float dt_seconds);
        void sync_backend_to_entities(World& world);
        Uuid try_get_entity_for_rigidbody(PhysicsRigidbodyHandle rigidbody) const;
        void on_asset_reload(const AssetReloadContext& context);

      private:
        void destroy_record(PhysicsEntityRecord& record);

      private:
        std::weak_ptr<IPhysicsBackend> _backend = {};
        std::weak_ptr<AssetManager> _asset_manager = {};
        std::weak_ptr<AssetReloadQueue> _reload_queue = {};
        std::weak_ptr<WorldManager> _world_manager = {};
        std::unordered_map<Uuid, PhysicsEntityRecord> _records_by_entity = {};
        std::unordered_map<uint64, Uuid> _entity_by_rigidbody_handle = {};
        std::unordered_map<Uuid, std::unordered_set<Uuid>> _overlap_entities_by_trigger = {};
        std::unordered_set<Uuid> _pending_model_reloads = {};
        Uuid _asset_reload_handler = {};
    };
}
