#pragma once
#include "tbx/interfaces/physics_backend.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/assets/messages.h"
#include "tbx/systems/ecs/world/manager.h"
#include "tbx/interfaces/message_dispatcher.h"
#include "tbx/systems/physics/settings.h"
#include "tbx/types/assets/world.h"
#include "tbx/types/raycast.h"
#include <memory>

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
            std::weak_ptr<WorldManager> world_manager,
            const PhysicsSettings& settings);
        Physics(
            std::weak_ptr<IPhysicsBackend> backend,
            std::weak_ptr<AssetManager> asset_manager,
            std::weak_ptr<WorldManager> world_manager,
            std::weak_ptr<IMessageCoordinator> message_coordinator,
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
        void on_asset_reloaded(const AssetReloadedEvent& event);

      private:
        struct EntityRecord;
        struct EntityRecordDeleter
        {
            void operator()(EntityRecord* record) const noexcept;
        };
        using EntityRecordPtr = std::unique_ptr<EntityRecord, EntityRecordDeleter>;

        void destroy_record(EntityRecord& record);

      private:
        std::weak_ptr<IPhysicsBackend> _backend = {};
        std::weak_ptr<AssetManager> _asset_manager = {};
        std::weak_ptr<IMessageCoordinator> _message_coordinator = {};
        std::weak_ptr<WorldManager> _world_manager = {};
        std::unordered_map<Uuid, EntityRecordPtr> _records_by_entity = {};
        std::unordered_map<uint64, Uuid> _entity_by_rigidbody_handle = {};
        std::unordered_map<Uuid, std::unordered_set<Uuid>> _overlap_entities_by_trigger = {};
        std::unordered_set<Uuid> _pending_model_reloads = {};
        Uuid _asset_reload_handler = {};
    };
}
