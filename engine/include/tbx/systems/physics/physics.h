#pragma once
#include "tbx/interfaces/physics_backend.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/assets/messages.h"
#include "tbx/systems/async/thread_manager.h"
#include "tbx/systems/world/manager.h"
#include "tbx/interfaces/message_dispatcher.h"
#include "tbx/systems/physics/settings.h"
#include "tbx/types/assets/world.h"
#include "tbx/types/raycast.h"
#include "tbx/types/typedefs.h"
#include "tbx/types/uuid.h"
#include "tbx/types/vectors.h"
#include <future>
#include <memory>
#include <vector>

namespace tbx
{
    class JobSystem;

    /// @brief
    /// Purpose: Application-owned physics service that synchronizes ECS components with the
    /// registered physics backend.
    /// @details
    /// Ownership: Borrows application services and owns entity-to-backend resource state.
    /// Thread Safety: The public API (`update`, `raycast`) must be called from the application main
    /// thread. Internally the heavy backend simulation step runs on a dedicated worker lane; the
    /// step touches only backend state (never the ECS), and `update`/`raycast`/shutdown all join the
    /// in-flight step before touching the backend, so callers never observe the worker thread.
    class TBX_API Physics final
    {
      public:
        Physics(
            std::weak_ptr<IPhysicsBackend> backend,
            std::weak_ptr<AssetManager> asset_manager,
            std::weak_ptr<WorldManager> world_manager,
            std::weak_ptr<ThreadManager> thread_manager,
            std::weak_ptr<JobSystem> job_system,
            const PhysicsSettings& settings);
        Physics(
            std::weak_ptr<IPhysicsBackend> backend,
            std::weak_ptr<AssetManager> asset_manager,
            std::weak_ptr<WorldManager> world_manager,
            std::weak_ptr<ThreadManager> thread_manager,
            std::weak_ptr<JobSystem> job_system,
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

        /// @brief Returns the wireframe triangles of the entity's backend collider shape — three
        /// shape-local vertices per triangle, any baked scale included: the cooked shape the
        /// simulation actually uses (e.g. a convex mesh collider's hull), for debug drawing. Empty
        /// when the entity has no physics body yet or the backend cannot produce geometry.
        /// Main-thread only; joins the in-flight simulation step (like raycast), so cache the result
        /// rather than querying per frame.
        std::vector<Vec3> get_shape(const Uuid& entity_id) const;

        void update(const DeltaTime& dt, const PhysicsSettings& settings);

        /// @brief Abandons the in-flight simulation step and destroys every backend body, so the
        /// simulation starts fresh from the live entity transforms on the next update. Used when the
        /// world is wholesale replaced (e.g. an editor leaving play mode restores its pre-play
        /// snapshot) so no body position, velocity, or uncommitted step carries across the reset.
        void reset();

      private:
        void clear_resources();
        static PhysicsBackendSettings get_backend_settings(const PhysicsSettings& settings);
        void process_contact_events(const std::vector<std::shared_ptr<World>>& worlds);
        void process_trigger_colliders(World& world);
        void sync_entities_to_backend(World& world, float dt_seconds);
        void sync_backend_to_entities(World& world);
        Uuid try_get_entity_for_rigidbody(PhysicsRigidbodyHandle rigidbody) const;
        void on_asset_reloaded(const AssetReloadedEvent& event);

        // Dispatches the backend simulation step onto the physics lane and records it as in-flight.
        void dispatch_step(const PhysicsSettings& settings, const DeltaTime& dt);
        // Joins the in-flight backend step, if any, so the backend is safe to touch on this thread.
        void wait_for_pending_step() const noexcept;

      private:
        struct EntityRecord;
        struct EntityRecordDeleter
        {
            void operator()(EntityRecord* record) const noexcept;
        };
        using EntityRecordPtr = std::unique_ptr<EntityRecord, EntityRecordDeleter>;

        // One entry per tracked body for the read-back pass: the heavy per-body backend state read
        // is gathered into `state` in parallel, then applied to the ECS serially. `record` points at
        // a _records_by_entity entry (stable for the duration of a single sync).
        struct SyncReadback
        {
            Uuid entity_id = {};
            EntityRecord* record = nullptr;
            PhysicsRigidbodyState state = {};
        };

        void destroy_record(EntityRecord& record);

      private:
        std::weak_ptr<IPhysicsBackend> _backend = {};
        std::weak_ptr<AssetManager> _asset_manager = {};
        std::weak_ptr<IMessageCoordinator> _message_coordinator = {};
        std::weak_ptr<WorldManager> _world_manager = {};
        std::weak_ptr<ThreadManager> _thread_manager = {};
        std::weak_ptr<JobSystem> _job_system = {};
        std::unordered_map<Uuid, EntityRecordPtr> _records_by_entity = {};
        // Reused across frames so the read-back pass does no per-frame heap allocation.
        std::vector<SyncReadback> _sync_readback = {};
        // Reused across frames so the contact drain does no per-frame heap allocation.
        std::vector<PhysicsContactEvent> _contact_events = {};
        std::unordered_map<uint64, Uuid> _entity_by_rigidbody_handle = {};
        std::unordered_map<Uuid, std::unordered_set<Uuid>> _overlap_entities_by_trigger = {};
        std::unordered_set<Uuid> _pending_model_reloads = {};
        Uuid _asset_reload_handler = {};

        // The backend simulation step runs on this lane so its work overlaps the rest of the frame
        // (rendering, asset work) instead of blocking the main thread. Only the main thread mutates
        // the members below, and it always joins `_pending_step` before touching the backend again.
        bool _has_physics_lane = false;
        mutable std::future<void> _pending_step = {};
        bool _results_pending = false;
    };
}
