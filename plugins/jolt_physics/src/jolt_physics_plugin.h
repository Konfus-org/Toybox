#pragma once
#include "tbx/math/quaternions.h"
#include "tbx/math/vectors.h"
#include "tbx/plugin_api/plugin.h"
#include <Jolt/Jolt.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <memory>
#include <unordered_map>

namespace tbx::plugins
{
    /// <summary>Runs rigid-body simulation for entities that contain Physics and Transform
    /// components and supports collider-only entities as static collision bodies.</summary>
    /// <remarks>Purpose: Owns the Jolt world used by this plugin and synchronizes ECS component
    /// state. Ownership: Owns Jolt simulation resources and body mappings; borrows host
    /// subsystems. Thread Safety: Not thread-safe; expected to run on the main update
    /// thread.</remarks>
    class JoltPhysicsPlugin final : public Plugin
    {
      public:
        void on_attach(IPluginHost& host) override;
        void on_detach() override;
        void on_fixed_update(const DeltaTime& dt) override;

      private:
        void clear_bodies();
        void apply_world_settings();
        void sync_entities_to_world(float dt_seconds);
        void sync_world_to_entities();

      private:
        struct BodyRecord
        {
            JPH::BodyID body_id = {};
            bool is_physics_driven = false;
            Vec3 last_position = Vec3(0.0F, 0.0F, 0.0F);
            Quat last_rotation = Quat(1.0F, 0.0F, 0.0F, 0.0F);
            Vec3 last_scale = Vec3(1.0F, 1.0F, 1.0F);
            bool has_last_transform = false;
        };

        JPH::PhysicsSystem _physics_system = {};
        std::unique_ptr<JPH::TempAllocator> _temp_allocator = nullptr;
        std::unique_ptr<JPH::JobSystemThreadPool> _job_system = nullptr;
        std::unordered_map<Uuid, BodyRecord> _bodies_by_entity = {};
        bool _is_ready = false;
    };
}
