#include "jolt_physics_backend.h"
#include "jolt_collision_layers.h"
#include "jolt_runtime_lifetime.h"
#include "tbx/systems/debugging/macros.h"
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>

namespace jolt_physics
{
    static std::uint32_t get_body_key(const JPH::BodyID& body_id)
    {
        return body_id.GetIndexAndSequenceNumber();
    }

    static JPH::Vec3 to_jolt_vec3(const tbx::Vec3& value)
    {
        return JPH::Vec3(value.x, value.y, value.z);
    }

    static JPH::RVec3 to_jolt_rvec3(const tbx::Vec3& value)
    {
        return JPH::RVec3(value.x, value.y, value.z);
    }

    static JPH::Quat to_jolt_quat(const tbx::Quat& value)
    {
        return JPH::Quat(value.x, value.y, value.z, value.w);
    }

    static tbx::Vec3 to_tbx_vec3(JPH::Vec3Arg value)
    {
        return tbx::Vec3(value.GetX(), value.GetY(), value.GetZ());
    }

    static tbx::Vec3 to_tbx_vec3_from_rvec3(JPH::RVec3Arg value)
    {
        return tbx::Vec3(
            static_cast<float>(value.GetX()),
            static_cast<float>(value.GetY()),
            static_cast<float>(value.GetZ()));
    }

    static tbx::Quat to_tbx_quat(JPH::QuatArg value)
    {
        return tbx::Quat(value.GetW(), value.GetX(), value.GetY(), value.GetZ());
    }

    static tbx::Vec3 get_safe_normalized(const tbx::Vec3& value, const tbx::Vec3& fallback)
    {
        const float length_squared = value.x * value.x + value.y * value.y + value.z * value.z;
        if (length_squared <= 0.000001F)
            return fallback;

        return value * (1.0F / std::sqrt(length_squared));
    }

    static JPH::RefConst<JPH::Shape> create_shape(const tbx::PhysicsColliderCreateInfo& create_info)
    {
        if (create_info.shape_type == tbx::PhysicsColliderShapeType::SPHERE)
            return new JPH::SphereShape(std::max(0.001F, create_info.radius));

        if (create_info.shape_type == tbx::PhysicsColliderShapeType::CAPSULE)
            return new JPH::CapsuleShape(
                std::max(0.001F, create_info.half_height),
                std::max(0.001F, create_info.radius));

        if (create_info.shape_type == tbx::PhysicsColliderShapeType::BOX)
        {
            return new JPH::BoxShape(
                JPH::Vec3(
                    std::max(0.001F, create_info.half_extents.x),
                    std::max(0.001F, create_info.half_extents.y),
                    std::max(0.001F, create_info.half_extents.z)));
        }

        if (create_info.shape_type != tbx::PhysicsColliderShapeType::MESH
            || create_info.mesh_vertices.empty())
            return new JPH::BoxShape(JPH::Vec3(0.5F, 0.5F, 0.5F));

        if (create_info.is_convex)
        {
            JPH::Array<JPH::Vec3> convex_points = {};
            convex_points.reserve(static_cast<JPH::uint>(create_info.mesh_vertices.size()));
            for (const auto& vertex : create_info.mesh_vertices)
                convex_points.push_back(to_jolt_vec3(vertex));

            auto convex_shape_result = JPH::ConvexHullShapeSettings(convex_points).Create();
            if (convex_shape_result.HasError())
            {
                TBX_TRACE_WARNING(
                    "Jolt physics: failed to build convex mesh collider: {}",
                    convex_shape_result.GetError().c_str());
                return new JPH::BoxShape(JPH::Vec3(0.5F, 0.5F, 0.5F));
            }

            return convex_shape_result.Get();
        }

        if (create_info.mesh_triangles.empty())
            return new JPH::BoxShape(JPH::Vec3(0.5F, 0.5F, 0.5F));

        JPH::VertexList vertex_list = {};
        vertex_list.reserve(static_cast<JPH::uint>(create_info.mesh_vertices.size()));
        for (const auto& vertex : create_info.mesh_vertices)
            vertex_list.push_back(JPH::Float3(vertex.x, vertex.y, vertex.z));

        JPH::IndexedTriangleList triangle_list = {};
        triangle_list.reserve(static_cast<JPH::uint>(create_info.mesh_triangles.size()));
        for (const auto& triangle : create_info.mesh_triangles)
        {
            triangle_list.push_back(
                JPH::IndexedTriangle(triangle.index0, triangle.index1, triangle.index2, 0U));
        }

        auto mesh_shape_result =
            JPH::MeshShapeSettings(std::move(vertex_list), std::move(triangle_list)).Create();
        if (mesh_shape_result.HasError())
        {
            TBX_TRACE_WARNING(
                "Jolt physics: failed to build mesh collider: {}",
                mesh_shape_result.GetError().c_str());
            return new JPH::BoxShape(JPH::Vec3(0.5F, 0.5F, 0.5F));
        }

        return mesh_shape_result.Get();
    }

    static JPH::EMotionType get_motion_type(const tbx::PhysicsRigidbodyCreateInfo& create_info)
    {
        if (!create_info.has_rigidbody)
            return JPH::EMotionType::Static;

        if (create_info.rigidbody.is_kinematic)
            return JPH::EMotionType::Kinematic;

        return JPH::EMotionType::Dynamic;
    }

    static void apply_dynamic_body_settings(
        const tbx::PhysicsBackendSettings& settings,
        const tbx::Rigidbody& rigidbody,
        bool is_trigger_only,
        JPH::BodyCreationSettings& out_body_settings)
    {
        out_body_settings.mIsSensor = is_trigger_only;
        out_body_settings.mAllowSleeping = rigidbody.is_sleep_enabled;
        out_body_settings.mFriction = rigidbody.friction;
        out_body_settings.mRestitution = rigidbody.restitution;
        out_body_settings.mLinearDamping = rigidbody.linear_damping;
        out_body_settings.mAngularDamping = rigidbody.angular_damping;
        out_body_settings.mLinearVelocity = to_jolt_vec3(rigidbody.linear_velocity);
        out_body_settings.mAngularVelocity = to_jolt_vec3(rigidbody.angular_velocity);
        out_body_settings.mGravityFactor = rigidbody.is_gravity_enabled ? 1.0F : 0.0F;
        out_body_settings.mMaxLinearVelocity = std::max(0.0F, settings.max_linear_velocity);
        out_body_settings.mMaxAngularVelocity = std::max(0.0F, settings.max_angular_velocity);
        out_body_settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        out_body_settings.mMassPropertiesOverride.mMass = rigidbody.mass;

        constexpr float ccd_linear_speed_threshold = 8.0F;
        const float linear_speed_squared =
            rigidbody.linear_velocity.x * rigidbody.linear_velocity.x
            + rigidbody.linear_velocity.y * rigidbody.linear_velocity.y
            + rigidbody.linear_velocity.z * rigidbody.linear_velocity.z;
        const float ccd_threshold_squared = ccd_linear_speed_threshold * ccd_linear_speed_threshold;
        out_body_settings.mMotionQuality = linear_speed_squared >= ccd_threshold_squared
                                               ? JPH::EMotionQuality::LinearCast
                                               : JPH::EMotionQuality::Discrete;
    }

    JoltPhysicsBackend::~JoltPhysicsBackend() noexcept
    {
        shutdown();
    }

    void JoltPhysicsBackend::initialize(const tbx::PhysicsBackendSettings& settings)
    {
        if (_is_ready)
            return;

        if (!JoltRuntimeLifetime::acquire())
        {
            TBX_TRACE_ERROR("Jolt physics: failed to initialize Jolt runtime.");
            return;
        }

        constexpr JPH::uint temp_allocator_bytes = 64U * 1024U * 1024U;
        _temp_allocator =
            std::make_unique<JPH::TempAllocatorImplWithMallocFallback>(temp_allocator_bytes);
        _job_system = std::make_unique<JPH::JobSystemThreadPool>(
            JPH::cMaxPhysicsJobs,
            JPH::cMaxPhysicsBarriers);

        _physics_system.Init(
            std::max<std::uint32_t>(1U, settings.max_body_count),
            0,
            std::max<std::uint32_t>(1U, settings.max_body_pairs),
            std::max<std::uint32_t>(1U, settings.max_contact_constraints),
            get_broad_phase_layer_interface(),
            get_object_vs_broad_phase_layer_filter(),
            get_object_layer_pair_filter());

        _settings = settings;
        apply_settings(_settings);
        _is_ready = true;
    }

    void JoltPhysicsBackend::shutdown()
    {
        if (!_is_ready && !_temp_allocator && !_job_system)
            return;

        clear_resources();
        _job_system.reset();
        _temp_allocator.reset();
        _is_ready = false;
        JoltRuntimeLifetime::release();
    }

    void JoltPhysicsBackend::update(
        const tbx::PhysicsBackendSettings& settings,
        const tbx::DeltaTime& dt)
    {
        if (!_is_ready || !_temp_allocator || !_job_system)
            return;

        _settings = settings;
        apply_settings(_settings);
        JPH::EPhysicsUpdateError update_error = _physics_system.Update(
            static_cast<float>(std::max(0.0001, dt.seconds)),
            1,
            _temp_allocator.get(),
            _job_system.get());
        if (update_error != JPH::EPhysicsUpdateError::None)
        {
            TBX_TRACE_WARNING(
                "Jolt physics update reported error flags: {}",
                static_cast<std::uint32_t>(update_error));
        }
    }

    bool JoltPhysicsBackend::raycast(
        const tbx::RaycastQuery& raycast_query,
        tbx::PhysicsRigidbodyHandle ignored_rigidbody,
        tbx::PhysicsRaycastHit& out_hit) const
    {
        out_hit = {};
        if (!_is_ready)
            return false;

        const tbx::Vec3 ray_direction =
            get_safe_normalized(raycast_query.direction, tbx::Vec3(0.0F, 0.0F, -1.0F));
        const float max_distance = std::max(0.0F, raycast_query.max_distance);
        if (max_distance <= 0.0F)
            return false;

        const JPH::RRayCast ray = JPH::RRayCast(
            to_jolt_rvec3(raycast_query.origin),
            to_jolt_vec3(ray_direction * max_distance));

        const auto& narrow_phase_query = _physics_system.GetNarrowPhaseQuery();
        JPH::RayCastResult ray_hit = {};
        bool has_hit = false;
        if (ignored_rigidbody.is_valid())
        {
            auto ignored_it = _rigidbodies.find(ignored_rigidbody.value);
            if (ignored_it != _rigidbodies.end())
            {
                JPH::IgnoreSingleBodyFilter ignore_body_filter =
                    JPH::IgnoreSingleBodyFilter(ignored_it->second.body_id);
                has_hit = narrow_phase_query.CastRay(ray, ray_hit, {}, {}, ignore_body_filter);
            }
            else
            {
                has_hit = narrow_phase_query.CastRay(ray, ray_hit);
            }
        }
        else
        {
            has_hit = narrow_phase_query.CastRay(ray, ray_hit);
        }

        out_hit.has_hit = has_hit;
        if (!has_hit)
            return true;

        out_hit.rigidbody = try_get_rigidbody_for_body(ray_hit.mBodyID);
        out_hit.hit_fraction = ray_hit.mFraction;
        out_hit.hit_position = to_tbx_vec3_from_rvec3(ray.GetPointOnRay(ray_hit.mFraction));
        return true;
    }

    tbx::PhysicsColliderHandle JoltPhysicsBackend::create_collider(
        const tbx::PhysicsColliderCreateInfo& create_info)
    {
        if (!_is_ready)
            return {};

        JPH::RefConst<JPH::Shape> shape = create_shape(create_info);
        if (!shape)
            return {};

        tbx::PhysicsColliderHandle handle = tbx::PhysicsColliderHandle {
            .value = _next_collider_handle++,
        };
        _colliders[handle.value] = JoltColliderResource {
            .shape = shape,
            .is_trigger_only = create_info.is_trigger_only,
        };
        return handle;
    }

    void JoltPhysicsBackend::destroy_collider(tbx::PhysicsColliderHandle collider)
    {
        if (!collider.is_valid())
            return;

        _colliders.erase(collider.value);
    }

    void JoltPhysicsBackend::update_collider(
        tbx::PhysicsColliderHandle collider,
        const tbx::PhysicsColliderCreateInfo& update_info)
    {
        auto collider_it = _colliders.find(collider.value);
        if (collider_it == _colliders.end())
            return;

        JPH::RefConst<JPH::Shape> shape = create_shape(update_info);
        if (!shape)
            return;

        collider_it->second.shape = shape;
        collider_it->second.is_trigger_only = update_info.is_trigger_only;

        auto& body_interface = _physics_system.GetBodyInterface();
        for (auto& rigidbody_entry : _rigidbodies)
        {
            if (rigidbody_entry.second.collider != collider)
                continue;

            if (!body_interface.IsAdded(rigidbody_entry.second.body_id))
                continue;

            body_interface
                .SetShape(rigidbody_entry.second.body_id, shape, true, JPH::EActivation::Activate);
        }
    }

    tbx::PhysicsRigidbodyHandle JoltPhysicsBackend::create_rigidbody(
        const tbx::PhysicsRigidbodyCreateInfo& create_info)
    {
        if (!_is_ready || !create_info.collider.is_valid())
            return {};

        auto collider_it = _colliders.find(create_info.collider.value);
        if (collider_it == _colliders.end() || !collider_it->second.shape)
            return {};

        const auto object_layer =
            create_info.has_rigidbody ? get_moving_object_layer() : get_static_object_layer();
        auto body_settings = JPH::BodyCreationSettings(
            collider_it->second.shape,
            to_jolt_rvec3(create_info.transform.position),
            to_jolt_quat(create_info.transform.rotation),
            get_motion_type(create_info),
            object_layer);
        body_settings.mIsSensor = create_info.is_trigger_only;

        if (create_info.has_rigidbody)
        {
            apply_dynamic_body_settings(
                _settings,
                create_info.rigidbody,
                create_info.is_trigger_only,
                body_settings);
        }

        auto activation = JPH::EActivation::DontActivate;
        if (create_info.has_rigidbody && !create_info.rigidbody.is_kinematic)
            activation = JPH::EActivation::Activate;

        JPH::BodyID body_id =
            _physics_system.GetBodyInterface().CreateAndAddBody(body_settings, activation);
        if (body_id.IsInvalid())
            return {};

        tbx::PhysicsRigidbodyHandle handle = tbx::PhysicsRigidbodyHandle {
            .value = _next_rigidbody_handle++,
        };
        _rigidbodies[handle.value] = JoltRigidbodyResource {
            .body_id = body_id,
            .collider = create_info.collider,
            .last_transform = create_info.transform,
        };
        _rigidbody_by_body_key[get_body_key(body_id)] = handle;
        return handle;
    }

    void JoltPhysicsBackend::destroy_rigidbody(tbx::PhysicsRigidbodyHandle rigidbody)
    {
        auto rigidbody_it = _rigidbodies.find(rigidbody.value);
        if (rigidbody_it == _rigidbodies.end())
            return;

        auto& body_interface = _physics_system.GetBodyInterface();
        const JPH::BodyID body_id = rigidbody_it->second.body_id;
        if (body_interface.IsAdded(body_id))
        {
            body_interface.RemoveBody(body_id);
            body_interface.DestroyBody(body_id);
        }

        _rigidbody_by_body_key.erase(get_body_key(body_id));
        _rigidbodies.erase(rigidbody_it);
    }

    tbx::PhysicsRigidbodyState JoltPhysicsBackend::get_rigidbody_state(
        tbx::PhysicsRigidbodyHandle rigidbody) const
    {
        auto rigidbody_it = _rigidbodies.find(rigidbody.value);
        if (rigidbody_it == _rigidbodies.end())
            return {};

        const auto& body_interface = _physics_system.GetBodyInterface();
        const JPH::BodyID body_id = rigidbody_it->second.body_id;
        if (!body_interface.IsAdded(body_id))
            return {};

        auto state = tbx::PhysicsRigidbodyState {};
        state.is_valid = true;
        state.transform = rigidbody_it->second.last_transform;
        state.transform.position = to_tbx_vec3_from_rvec3(body_interface.GetPosition(body_id));
        state.transform.rotation = to_tbx_quat(body_interface.GetRotation(body_id));
        state.linear_velocity = to_tbx_vec3(body_interface.GetLinearVelocity(body_id));
        state.angular_velocity = to_tbx_vec3(body_interface.GetAngularVelocity(body_id));
        return state;
    }

    void JoltPhysicsBackend::get_rigidbody_overlaps(
        tbx::PhysicsRigidbodyHandle rigidbody,
        std::vector<tbx::PhysicsRigidbodyHandle>& out_overlaps) const
    {
        out_overlaps.clear();
        auto rigidbody_it = _rigidbodies.find(rigidbody.value);
        if (rigidbody_it == _rigidbodies.end())
            return;

        const auto& body_interface = _physics_system.GetBodyInterface();
        const auto& narrow_phase_query = _physics_system.GetNarrowPhaseQuery();
        const JPH::BodyID body_id = rigidbody_it->second.body_id;
        if (!body_interface.IsAdded(body_id))
            return;

        JPH::RefConst<JPH::Shape> shape = body_interface.GetShape(body_id);
        if (!shape)
            return;

        JPH::CollideShapeSettings collide_settings = {};
        collide_settings.mBackFaceMode = JPH::EBackFaceMode::CollideWithBackFaces;
        JPH::IgnoreSingleBodyFilter ignore_self_filter = JPH::IgnoreSingleBodyFilter(body_id);
        JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector = {};

        narrow_phase_query.CollideShape(
            shape.GetPtr(),
            JPH::Vec3::sReplicate(1.0F),
            body_interface.GetCenterOfMassTransform(body_id),
            collide_settings,
            JPH::RVec3::sZero(),
            collector,
            {},
            {},
            ignore_self_filter);

        out_overlaps.reserve(static_cast<size>(collector.mHits.size()));
        for (const auto& overlap_hit : collector.mHits)
        {
            tbx::PhysicsRigidbodyHandle overlapped_rigidbody =
                try_get_rigidbody_for_body(overlap_hit.mBodyID2);
            if (overlapped_rigidbody.is_valid())
                out_overlaps.push_back(overlapped_rigidbody);
        }
    }

    void JoltPhysicsBackend::update_rigidbody(
        tbx::PhysicsRigidbodyHandle rigidbody,
        const tbx::PhysicsRigidbodyUpdateInfo& update_info)
    {
        auto rigidbody_it = _rigidbodies.find(rigidbody.value);
        if (rigidbody_it == _rigidbodies.end())
            return;

        auto& body_interface = _physics_system.GetBodyInterface();
        const JPH::BodyID body_id = rigidbody_it->second.body_id;
        if (!body_interface.IsAdded(body_id))
            return;

        if (!update_info.has_rigidbody)
        {
            body_interface.SetPositionAndRotation(
                body_id,
                to_jolt_rvec3(update_info.transform.position),
                to_jolt_quat(update_info.transform.rotation),
                JPH::EActivation::DontActivate);
            rigidbody_it->second.last_transform = update_info.transform;
            return;
        }

        const auto& rigidbody_component = update_info.rigidbody;
        if (rigidbody_component.is_kinematic)
        {
            body_interface.MoveKinematic(
                body_id,
                to_jolt_rvec3(update_info.transform.position),
                to_jolt_quat(update_info.transform.rotation),
                std::max(0.0001F, update_info.dt_seconds));
            body_interface.SetLinearVelocity(
                body_id,
                to_jolt_vec3(rigidbody_component.linear_velocity));
            body_interface.SetAngularVelocity(
                body_id,
                to_jolt_vec3(rigidbody_component.angular_velocity));
            rigidbody_it->second.last_transform = update_info.transform;
        }
        else if (update_info.is_transform_dirty)
        {
            if (rigidbody_component.transform_sync_mode == tbx::PhysicsTransformSyncMode::TELEPORT)
            {
                body_interface.SetPositionAndRotation(
                    body_id,
                    to_jolt_rvec3(update_info.transform.position),
                    to_jolt_quat(update_info.transform.rotation),
                    JPH::EActivation::Activate);
            }
            else if (
                rigidbody_component.transform_sync_mode == tbx::PhysicsTransformSyncMode::SWEEP)
            {
                body_interface.SetLinearVelocity(
                    body_id,
                    to_jolt_vec3(update_info.sweep_linear_velocity));
                body_interface.SetAngularVelocity(
                    body_id,
                    to_jolt_vec3(update_info.sweep_angular_velocity));
                body_interface.ActivateBody(body_id);
            }
        }

        body_interface.SetFriction(body_id, rigidbody_component.friction);
        body_interface.SetRestitution(body_id, rigidbody_component.restitution);
        body_interface.SetGravityFactor(
            body_id,
            rigidbody_component.is_gravity_enabled ? 1.0F : 0.0F);
    }

    void JoltPhysicsBackend::apply_settings(const tbx::PhysicsBackendSettings& settings)
    {
        auto jolt_settings = _physics_system.GetPhysicsSettings();
        jolt_settings.mNumVelocitySteps =
            std::max<std::uint32_t>(2U, settings.solver_velocity_iterations);
        jolt_settings.mNumPositionSteps =
            std::max<std::uint32_t>(1U, settings.solver_position_iterations);

        _physics_system.SetPhysicsSettings(jolt_settings);
        _physics_system.SetGravity(to_jolt_vec3(settings.gravity));
    }

    void JoltPhysicsBackend::clear_resources()
    {
        auto rigidbody_handles = std::vector<tbx::PhysicsRigidbodyHandle> {};
        rigidbody_handles.reserve(_rigidbodies.size());
        for (const auto& rigidbody_entry : _rigidbodies)
        {
            rigidbody_handles.push_back(
                tbx::PhysicsRigidbodyHandle {.value = rigidbody_entry.first});
        }

        for (tbx::PhysicsRigidbodyHandle rigidbody : rigidbody_handles)
            destroy_rigidbody(rigidbody);

        _colliders.clear();
        _rigidbody_by_body_key.clear();
    }

    tbx::PhysicsRigidbodyHandle JoltPhysicsBackend::try_get_rigidbody_for_body(
        const JPH::BodyID& body_id) const
    {
        auto rigidbody_it = _rigidbody_by_body_key.find(get_body_key(body_id));
        if (rigidbody_it == _rigidbody_by_body_key.end())
            return {};

        return rigidbody_it->second;
    }
}
