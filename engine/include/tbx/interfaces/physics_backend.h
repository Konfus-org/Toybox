#pragma once
#include "tbx/systems/time/delta_time.h"
#include "tbx/tbx_api.h"
#include "tbx/types/components/collider.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/components/rigidbody.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/handle.h"
#include "tbx/types/raycast.h"
#include "tbx/types/typedefs.h"
#include <vector>

namespace tbx
{
    using PhysicsHandle = Handle;

    struct PhysicsRaycastHit
    {
        bool has_hit = false;
        PhysicsHandle rigidbody = {};
        Vec3 hit_position = Vec3(0.0F, 0.0F, 0.0F);

        operator bool() const
        {
            return has_hit && rigidbody.is_valid();
        }
    };

    /// @brief
    /// Purpose: One solid contact this body currently has with another body, reported through
    /// get_state. `normal`/`position` describe the contact point.
    struct PhysicsContact
    {
        PhysicsHandle other = {};
        Vec3 position = Vec3(0.0F, 0.0F, 0.0F);
        Vec3 normal = Vec3(0.0F, 0.0F, 0.0F);
    };

    /// @brief
    /// Purpose: The per-body simulation snapshot pulled every frame via get_state: pose, velocity,
    /// and the body's current solid contacts and (when it is a trigger/sensor) overlaps.
    /// @details
    /// Ownership: Value type; callers own the copy filled by get_state.
    struct PhysicsEntityState
    {
        Transform transform = {};
        Vec3 linear_velocity = Vec3(0.0F, 0.0F, 0.0F);
        Vec3 angular_velocity = Vec3(0.0F, 0.0F, 0.0F);
        std::vector<PhysicsContact> contacts = {};
        std::vector<PhysicsHandle> overlaps = {};
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
        virtual void initialize(
            Vec3 gravity = Vec3(0.0F, -9.81F, 0.0F),
            uint32 max_body_count = 1024U,
            uint32 max_contact_constraints = 1024U,
            uint32 max_body_pairs = 1024U,
            uint32 solver_velocity_iterations = 10U,
            uint32 solver_position_iterations = 2U,
            float max_linear_velocity = 500.0F,
            float max_angular_velocity = 250.0F) = 0;
        virtual void shutdown() = 0;

        // Runtime-tweakable simulation settings. The init-only limits (body/pair/constraint counts)
        // live on initialize because the backend fixes them when it builds the simulation.
        virtual void set_gravity(Vec3 gravity) = 0;
        virtual void set_solver_velocity_iterations(uint32 iterations) = 0;
        virtual void set_solver_position_iterations(uint32 iterations) = 0;
        virtual void set_max_linear_velocity(float max_linear_velocity) = 0;
        virtual void set_max_angular_velocity(float max_angular_velocity) = 0;

        virtual void step(const DeltaTime& dt) = 0;

        /// @brief Fills `out_state` with the body's pose, velocity, contacts, and overlaps after the
        /// last step. Returns false when `handle` names no live body.
        virtual bool get_state(const PhysicsHandle& handle, PhysicsEntityState& out_state) = 0;

        virtual bool raycast(
            const RaycastQuery& raycast_query,
            const std::vector<PhysicsHandle>& ignored,
            PhysicsRaycastHit& out_hit) const = 0;

        /// @brief Builds the solid collision shape for the concrete collider (BoxCollider,
        /// SphereCollider, ...); `mesh` supplies geometry for mesh colliders only. The shape becomes
        /// the pending shape used by the next create_rigidbody. Returns a handle to the shape.
        virtual PhysicsHandle create_collider(
            const Collider& collider,
            const Mesh& mesh = Mesh::EMPTY) = 0;

        /// @brief Builds the sensor collision shape for the concrete trigger; `mesh` supplies geometry
        /// for mesh triggers only. The shape becomes the pending shape used by the next
        /// create_rigidbody. Returns a handle to the shape.
        virtual PhysicsHandle create_trigger(
            const Trigger& trigger,
            const Mesh& mesh = Mesh::EMPTY) = 0;

        /// @brief Places a body at `transform` using the pending shape from the preceding
        /// create_collider/create_trigger (a unit box when none). The body is static when
        /// `rigidbody` is invalid, kinematic when it is kinematic, otherwise dynamic; it is a sensor
        /// when the pending shape came from create_trigger. Returns the body handle.
        virtual PhysicsHandle create_rigidbody(
            Transform transform = {},
            Rigidbody rigidbody = {}) = 0;

        virtual void destroy(const PhysicsHandle& physics_handle) = 0;

        virtual std::vector<Vec3>& get_debug_shape(PhysicsHandle physics_handle) const = 0;
    };
}
