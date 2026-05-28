#pragma once
#include "tbx/interfaces/physics_backend.h"
#include "tbx/systems/app/settings.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/types/assets/world.h"
#include "tbx/types/raycast.h"

namespace tbx
{
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
        PhysicsBackendSettings get_backend_settings() const;
        void process_trigger_colliders(World& world);
        void sync_entities_to_backend(World& world, float dt_seconds);
        void sync_backend_to_entities(World& world);
        Uuid try_get_entity_for_rigidbody(PhysicsRigidbodyHandle rigidbody) const;

      private:
        struct EntityRecord;
        struct State;

        void destroy_record(EntityRecord& record);

      private:
        std::unique_ptr<State> _state = {};
    };
}
