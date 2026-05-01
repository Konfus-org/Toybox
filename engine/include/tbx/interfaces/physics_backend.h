#pragma once
#include "tbx/systems/math/transform.h"
#include "tbx/systems/physics/raycast.h"
#include "tbx/systems/physics/rigidbody.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/tbx_api.h"
#include "tbx/types/typedefs.h"
#include <vector>

namespace tbx
{
    struct PhysicsBackendSettings
    {
        Vec3 gravity = Vec3(0.0F, -9.81F, 0.0F);
        uint32 max_body_count = 1024U;
        uint32 max_contact_constraints = 1024U;
        uint32 max_body_pairs = 1024U;
        uint32 solver_velocity_iterations = 10U;
        uint32 solver_position_iterations = 2U;
        float max_linear_velocity = 500.0F;
        float max_angular_velocity = 250.0F;
    };

    struct PhysicsColliderHandle
    {
        uint64 value = 0U;

        bool is_valid() const
        {
            return value != 0U;
        }

        bool operator==(const PhysicsColliderHandle& other) const = default;
    };

    struct PhysicsRigidbodyHandle
    {
        uint64 value = 0U;

        bool is_valid() const
        {
            return value != 0U;
        }

        bool operator==(const PhysicsRigidbodyHandle& other) const = default;
    };

    enum class PhysicsColliderShapeType
    {
        BOX = 0,
        SPHERE = 1,
        CAPSULE = 2,
        MESH = 3,
    };

    struct PhysicsMeshTriangle
    {
        uint32 index0 = 0U;
        uint32 index1 = 0U;
        uint32 index2 = 0U;
    };

    struct PhysicsColliderCreateInfo
    {
        PhysicsColliderShapeType shape_type = PhysicsColliderShapeType::BOX;
        Vec3 half_extents = Vec3(0.5F, 0.5F, 0.5F);
        float radius = 0.5F;
        float half_height = 0.5F;
        bool is_convex = true;
        bool is_trigger_only = false;
        std::vector<Vec3> mesh_vertices = {};
        std::vector<PhysicsMeshTriangle> mesh_triangles = {};
    };

    struct PhysicsRigidbodyCreateInfo
    {
        PhysicsColliderHandle collider = {};
        Transform transform = {};
        Rigidbody rigidbody = {};
        bool has_rigidbody = false;
        bool is_trigger_only = false;
    };

    struct PhysicsRigidbodyUpdateInfo
    {
        Transform transform = {};
        Rigidbody rigidbody = {};
        bool has_rigidbody = false;
        bool is_trigger_only = false;
        bool is_transform_dirty = false;
        float dt_seconds = 0.0001F;
        Vec3 sweep_linear_velocity = Vec3(0.0F, 0.0F, 0.0F);
        Vec3 sweep_angular_velocity = Vec3(0.0F, 0.0F, 0.0F);
    };

    struct PhysicsRigidbodyState
    {
        bool is_valid = false;
        Transform transform = {};
        Vec3 linear_velocity = Vec3(0.0F, 0.0F, 0.0F);
        Vec3 angular_velocity = Vec3(0.0F, 0.0F, 0.0F);
    };

    struct PhysicsRaycastHit
    {
        bool has_hit = false;
        PhysicsRigidbodyHandle rigidbody = {};
        Vec3 hit_position = Vec3(0.0F, 0.0F, 0.0F);
        float hit_fraction = 1.0F;

        operator bool() const
        {
            return has_hit && rigidbody.is_valid();
        }
    };

    /// @brief
    /// Purpose: Backend interface implemented by physics plugins.
    /// @details
    /// Ownership: Implementations own backend resources and return opaque handles to callers.
    /// Thread Safety: Not thread-safe unless a backend explicitly documents otherwise.
    class TBX_API IPhysicsBackend
    {
      public:
        virtual ~IPhysicsBackend() noexcept = default;

      public:
        virtual void initialize(const PhysicsBackendSettings& settings) = 0;
        virtual void shutdown() = 0;
        virtual void update(const PhysicsBackendSettings& settings, const DeltaTime& dt) = 0;
        virtual bool raycast(
            const RaycastQuery& raycast_query,
            PhysicsRigidbodyHandle ignored_rigidbody,
            PhysicsRaycastHit& out_hit) const = 0;

        virtual PhysicsColliderHandle create_collider(
            const PhysicsColliderCreateInfo& create_info) = 0;
        virtual void destroy_collider(PhysicsColliderHandle collider) = 0;
        virtual void update_collider(
            PhysicsColliderHandle collider,
            const PhysicsColliderCreateInfo& update_info) = 0;

        virtual PhysicsRigidbodyHandle create_rigidbody(
            const PhysicsRigidbodyCreateInfo& create_info) = 0;
        virtual void destroy_rigidbody(PhysicsRigidbodyHandle rigidbody) = 0;
        virtual PhysicsRigidbodyState get_rigidbody_state(
            PhysicsRigidbodyHandle rigidbody) const = 0;
        virtual void get_rigidbody_overlaps(
            PhysicsRigidbodyHandle rigidbody,
            std::vector<PhysicsRigidbodyHandle>& out_overlaps) const = 0;
        virtual void update_rigidbody(
            PhysicsRigidbodyHandle rigidbody,
            const PhysicsRigidbodyUpdateInfo& update_info) = 0;
    };
}
