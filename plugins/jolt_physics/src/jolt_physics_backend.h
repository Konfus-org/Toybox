#pragma once
#include "jolt_contact_listener.h"
#include "tbx/interfaces/physics_backend.h"
#include "tbx/types/uuid.h"
#include <Jolt/Jolt.h>
#include <cstdint>

// clang-format off
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/PhysicsSystem.h>
// clang-format on

#include <memory>
#include <unordered_map>
#include <vector>

namespace jolt_physics
{
    // A cooked collision shape, created by create_collider/create_trigger and consumed by the next
    // create_rigidbody. Kept addressable by its own handle so the engine can destroy it and query
    // its debug geometry.
    struct JoltShapeResource
    {
        JPH::RefConst<JPH::Shape> shape = nullptr;
        bool is_sensor = false;
    };

    // A placed simulation body, created by create_rigidbody from the pending shape.
    struct JoltBodyResource
    {
        JPH::BodyID body_id = {};
        tbx::Uuid shape_id = {};
        bool is_sensor = false;
    };

    class JoltPhysicsBackend final : public tbx::IPhysicsBackend
    {
      public:
        ~JoltPhysicsBackend() noexcept override;

      public:
        void initialize(
            tbx::Vec3 gravity,
            uint32 max_body_count,
            uint32 max_contact_constraints,
            uint32 max_body_pairs,
            uint32 solver_velocity_iterations,
            uint32 solver_position_iterations,
            float max_linear_velocity,
            float max_angular_velocity) override;
        void shutdown() override;

        void set_gravity(tbx::Vec3 gravity) override;
        void set_solver_velocity_iterations(uint32 iterations) override;
        void set_solver_position_iterations(uint32 iterations) override;
        void set_max_linear_velocity(float max_linear_velocity) override;
        void set_max_angular_velocity(float max_angular_velocity) override;

        void step(const tbx::DeltaTime& dt) override;
        bool get_state(const tbx::PhysicsHandle& handle, tbx::PhysicsEntityState& out_state)
            override;

        bool raycast(
            const tbx::RaycastQuery& raycast_query,
            const std::vector<tbx::PhysicsHandle>& ignored,
            tbx::PhysicsRaycastHit& out_hit) const override;

        tbx::PhysicsHandle create_collider(const tbx::Collider& collider, const tbx::Mesh& mesh)
            override;
        tbx::PhysicsHandle create_trigger(const tbx::Trigger& trigger, const tbx::Mesh& mesh)
            override;
        tbx::PhysicsHandle create_rigidbody(tbx::Transform transform, tbx::Rigidbody rigidbody)
            override;
        void destroy(const tbx::PhysicsHandle& physics_handle) override;

        std::vector<tbx::Vec3>& get_debug_shape(tbx::PhysicsHandle physics_handle) const override;

      private:
        void apply_solver_settings();
        void clear_resources();
        tbx::PhysicsHandle store_shape(JPH::RefConst<JPH::Shape> shape, bool is_sensor);
        JPH::RefConst<JPH::Shape> take_pending_shape(bool& out_is_sensor, tbx::Uuid& out_shape_id);
        tbx::PhysicsHandle try_get_body_handle(std::uint32_t body_key) const;
        void gather_overlaps(
            const JoltBodyResource& body, std::vector<tbx::PhysicsHandle>& out_overlaps) const;

      private:
        JPH::PhysicsSystem _physics_system = {};
        JoltContactEventListener _contact_listener = {};
        std::unique_ptr<JPH::TempAllocator> _temp_allocator = nullptr;
        std::unique_ptr<JPH::JobSystemThreadPool> _job_system = nullptr;
        std::unordered_map<tbx::Uuid, JoltShapeResource> _shapes = {};
        std::unordered_map<tbx::Uuid, JoltBodyResource> _bodies = {};
        std::unordered_map<std::uint32_t, tbx::Uuid> _body_by_key = {};
        tbx::Uuid _pending_shape_id = {};

        tbx::Vec3 _gravity = tbx::Vec3(0.0F, -9.81F, 0.0F);
        uint32 _solver_velocity_iterations = 10U;
        uint32 _solver_position_iterations = 2U;
        float _max_linear_velocity = 500.0F;
        float _max_angular_velocity = 250.0F;

        // Reused across get_debug_shape calls so debug queries do no per-call heap churn.
        mutable std::vector<tbx::Vec3> _debug_shape = {};
        bool _is_ready = false;
    };
}
