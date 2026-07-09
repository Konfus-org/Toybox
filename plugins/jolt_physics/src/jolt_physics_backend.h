#pragma once
#include "jolt_contact_listener.h"
#include "tbx/interfaces/physics_backend.h"
#include <Jolt/Jolt.h>

// clang-format off
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/PhysicsSystem.h>
// clang-format on

namespace jolt_physics
{
    struct JoltColliderResource
    {
        JPH::RefConst<JPH::Shape> shape = nullptr;
        bool is_trigger_only = false;
    };

    struct JoltRigidbodyResource
    {
        JPH::BodyID body_id = {};
        tbx::PhysicsColliderHandle collider = {};
        tbx::Transform last_transform = {};
    };

    class JoltPhysicsBackend final : public tbx::IPhysicsBackend
    {
      public:
        ~JoltPhysicsBackend() noexcept override;

      public:
        void initialize(const tbx::PhysicsBackendSettings& settings) override;
        void shutdown() override;
        void update(const tbx::PhysicsBackendSettings& settings, const tbx::DeltaTime& dt) override;
        void drain_contact_events(std::vector<tbx::PhysicsContactEvent>& out_events) override;
        bool raycast(
            const tbx::RaycastQuery& raycast_query,
            tbx::PhysicsRigidbodyHandle ignored_rigidbody,
            tbx::PhysicsRaycastHit& out_hit) const override;

        tbx::PhysicsColliderHandle create_collider(
            const tbx::PhysicsColliderCreateInfo& create_info) override;
        void destroy_collider(tbx::PhysicsColliderHandle collider) override;
        void update_collider(
            tbx::PhysicsColliderHandle collider,
            const tbx::PhysicsColliderCreateInfo& update_info) override;
        bool get_shape(
            tbx::PhysicsColliderHandle collider,
            std::vector<tbx::Vec3>& out_triangle_vertices) const override;

        tbx::PhysicsRigidbodyHandle create_rigidbody(
            const tbx::PhysicsRigidbodyCreateInfo& create_info) override;
        void destroy_rigidbody(tbx::PhysicsRigidbodyHandle rigidbody) override;
        tbx::PhysicsRigidbodyState get_rigidbody_state(
            tbx::PhysicsRigidbodyHandle rigidbody) const override;
        void get_rigidbody_overlaps(
            tbx::PhysicsRigidbodyHandle rigidbody,
            std::vector<tbx::PhysicsRigidbodyHandle>& out_overlaps) const override;
        void update_rigidbody(
            tbx::PhysicsRigidbodyHandle rigidbody,
            const tbx::PhysicsRigidbodyUpdateInfo& update_info) override;

      private:
        void apply_settings(const tbx::PhysicsBackendSettings& settings);
        void clear_resources();
        bool is_trigger_only_body(tbx::PhysicsRigidbodyHandle rigidbody) const;
        tbx::PhysicsRigidbodyHandle try_get_rigidbody_for_body(const JPH::BodyID& body_id) const;

      private:
        JPH::PhysicsSystem _physics_system = {};
        JoltContactEventListener _contact_listener = {};
        std::unique_ptr<JPH::TempAllocator> _temp_allocator = nullptr;
        std::unique_ptr<JPH::JobSystemThreadPool> _job_system = nullptr;
        std::unordered_map<uint64, JoltColliderResource> _colliders = {};
        std::unordered_map<uint64, JoltRigidbodyResource> _rigidbodies = {};
        std::unordered_map<uint32, tbx::PhysicsRigidbodyHandle> _rigidbody_by_body_key = {};
        // Reused across drains so contact hand-off does no per-step heap allocation.
        std::vector<JoltContactRecord> _drained_contacts = {};
        tbx::PhysicsBackendSettings _settings = {};
        uint64 _next_collider_handle = 1U;
        uint64 _next_rigidbody_handle = 1U;
        bool _is_ready = false;
    };
}
