#include "jolt_physics_backend.h"
#include "internal/jolt_physics_backend_internal.h"
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
#include <algorithm>
#include <cmath>
namespace jolt_physics
{
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
            internal::get_safe_normalized(raycast_query.direction, tbx::Vec3(0.0F, 0.0F, -1.0F));
        const float max_distance = std::max(0.0F, raycast_query.max_distance);
        if (max_distance <= 0.0F)
            return false;

        const JPH::RRayCast ray = JPH::RRayCast(
            internal::to_jolt_rvec3(raycast_query.origin),
            internal::to_jolt_vec3(ray_direction * max_distance));

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
        out_hit.hit_position =
            internal::to_tbx_vec3_from_rvec3(ray.GetPointOnRay(ray_hit.mFraction));
        return true;
    }

    tbx::PhysicsColliderHandle JoltPhysicsBackend::create_collider(
        const tbx::PhysicsColliderCreateInfo& create_info)
    {
        if (!_is_ready)
            return {};

        JPH::RefConst<JPH::Shape> shape = internal::create_shape(create_info);
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

        JPH::RefConst<JPH::Shape> shape = internal::create_shape(update_info);
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
            internal::to_jolt_rvec3(create_info.transform.position),
            internal::to_jolt_quat(create_info.transform.rotation),
            internal::get_motion_type(create_info),
            object_layer);
        body_settings.mIsSensor = create_info.is_trigger_only;

        if (create_info.has_rigidbody)
        {
            internal::apply_dynamic_body_settings(
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
        _rigidbody_by_body_key[internal::get_body_key(body_id)] = handle;
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

        _rigidbody_by_body_key.erase(internal::get_body_key(body_id));
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
        state.transform.position =
            internal::to_tbx_vec3_from_rvec3(body_interface.GetPosition(body_id));
        state.transform.rotation = internal::to_tbx_quat(body_interface.GetRotation(body_id));
        state.linear_velocity = internal::to_tbx_vec3(body_interface.GetLinearVelocity(body_id));
        state.angular_velocity = internal::to_tbx_vec3(body_interface.GetAngularVelocity(body_id));
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
                internal::to_jolt_rvec3(update_info.transform.position),
                internal::to_jolt_quat(update_info.transform.rotation),
                JPH::EActivation::DontActivate);
            rigidbody_it->second.last_transform = update_info.transform;
            return;
        }

        const auto& rigidbody_component = update_info.rigidbody;
        if (rigidbody_component.is_kinematic)
        {
            body_interface.MoveKinematic(
                body_id,
                internal::to_jolt_rvec3(update_info.transform.position),
                internal::to_jolt_quat(update_info.transform.rotation),
                std::max(0.0001F, update_info.dt_seconds));
            body_interface.SetLinearVelocity(
                body_id,
                internal::to_jolt_vec3(rigidbody_component.linear_velocity));
            body_interface.SetAngularVelocity(
                body_id,
                internal::to_jolt_vec3(rigidbody_component.angular_velocity));
            rigidbody_it->second.last_transform = update_info.transform;
        }
        else if (update_info.is_transform_dirty)
        {
            if (rigidbody_component.transform_sync_mode == tbx::PhysicsTransformSyncMode::TELEPORT)
            {
                body_interface.SetPositionAndRotation(
                    body_id,
                    internal::to_jolt_rvec3(update_info.transform.position),
                    internal::to_jolt_quat(update_info.transform.rotation),
                    JPH::EActivation::Activate);
            }
            else if (
                rigidbody_component.transform_sync_mode == tbx::PhysicsTransformSyncMode::SWEEP)
            {
                body_interface.SetLinearVelocity(
                    body_id,
                    internal::to_jolt_vec3(update_info.sweep_linear_velocity));
                body_interface.SetAngularVelocity(
                    body_id,
                    internal::to_jolt_vec3(update_info.sweep_angular_velocity));
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
        _physics_system.SetGravity(internal::to_jolt_vec3(settings.gravity));
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
        auto rigidbody_it = _rigidbody_by_body_key.find(internal::get_body_key(body_id));
        if (rigidbody_it == _rigidbody_by_body_key.end())
            return {};

        return rigidbody_it->second;
    }
}
