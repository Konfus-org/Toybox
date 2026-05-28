#pragma once
#include "tbx/interfaces/physics_backend.h"
#include "tbx/systems/app/settings.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/types/assets/world.h"
#include "tbx/tbx_api.h"
#include "tbx/types/raycast.h"
#include "tbx/types/uuid.h"
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace tbx
{
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
            std::weak_ptr<AppSettings> settings);
        ~Physics() noexcept;

      public:
        Physics(const Physics&) = delete;
        Physics& operator=(const Physics&) = delete;
        Physics(Physics&&) noexcept = delete;
        Physics& operator=(Physics&&) noexcept = delete;

      public:
        RaycastResult raycast(const RaycastQuery& raycast_query) const;
        void update(const DeltaTime& dt);

      private:
        void clear_resources();
        void destroy_record(PhysicsEntityRecord& record);
        PhysicsBackendSettings get_backend_settings() const;
        void process_trigger_colliders(World& world);
        void sync_entities_to_backend(World& world, float dt_seconds);
        void sync_backend_to_entities(World& world);
        Uuid try_get_entity_for_rigidbody(PhysicsRigidbodyHandle rigidbody) const;

      private:
        std::weak_ptr<IPhysicsBackend> _backend;
        std::weak_ptr<AssetManager> _asset_manager;
        std::weak_ptr<AppSettings> _settings;
        std::unordered_map<Uuid, PhysicsEntityRecord> _records_by_entity = {};
        std::unordered_map<uint64, Uuid> _entity_by_rigidbody_handle = {};
        std::unordered_map<Uuid, std::unordered_set<Uuid>> _overlap_entities_by_trigger = {};
    };
}
