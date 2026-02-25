#pragma once
#include "tbx/math/quaternions.h"
#include "tbx/math/vectors.h"
#include "tbx/physics/raycast.h"
#include "tbx/plugin_api/plugin.h"
#include <Jolt/Jolt.h>
// clang-format off
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSystem.h>
// clang-format on
#include <cstdint>
#include <functional>
#include <memory>
#include <thread>
#include <unordered_map>
#include <unordered_set>

namespace tbx::plugins
{
    /// <summary>Runs rigid-body simulation for entities that contain Physics and Transform
    /// components and supports collider-only entities as static collision bodies.</summary>
    /// <remarks>Purpose: Owns the Jolt world used by this plugin and synchronizes ECS component
    /// state. Ownership: Owns Jolt simulation resources and body mappings; borrows host
    /// subsystems. Thread Safety: Uses a dedicated physics lane; public callbacks marshal work to
    /// that lane.</remarks>
    class JoltPhysicsPlugin final : public Plugin
    {
      public:
        ~JoltPhysicsPlugin() override;
        void on_attach(IPluginHost& host) override;
        void on_detach() override;
        void on_fixed_update(const DeltaTime& dt) override;
        void on_recieve_message(Message& msg) override;

      private:
        void clear_bodies();
        void apply_world_settings();
        void sync_entities_to_world(float dt_seconds);
        void sync_world_to_entities();
        void process_trigger_colliders();
        void handle_raycast_request(RaycastRequest& request) const;
        Uuid try_get_entity_for_body(const JPH::BodyID& body_id) const;
        bool is_on_physics_thread() const;
        void run_on_physics_lane_and_wait(const std::function<void()>& work);

      private:
        struct BodyRecord
        {
            JPH::BodyID body_id = {};
            bool is_physics_driven = false;
            Vec3 last_position = Vec3(0.0F, 0.0F, 0.0F);
            Quat last_rotation = Quat(1.0F, 0.0F, 0.0F, 0.0F);
            Vec3 last_scale = Vec3(1.0F, 1.0F, 1.0F);
            bool has_last_transform = false;
            bool is_trigger_only = false;
        };

        JPH::PhysicsSystem _physics_system = {};
        std::unique_ptr<JPH::TempAllocator> _temp_allocator = nullptr;
        std::unique_ptr<JPH::JobSystemThreadPool> _job_system = nullptr;
        std::unordered_map<Uuid, BodyRecord> _bodies_by_entity = {};
        std::unordered_map<std::uint32_t, Uuid> _entity_by_body_key = {};
        std::unordered_map<Uuid, std::unordered_set<Uuid>> _overlap_entities_by_trigger = {};
        std::thread::id _physics_thread_id = {};
        bool _is_ready = false;
    };
}
