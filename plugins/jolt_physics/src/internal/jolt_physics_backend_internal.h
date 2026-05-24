#pragma once
#include "jolt_collision_layers.h"
#include "jolt_physics_backend.h"
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

namespace jolt_physics::internal
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

}
