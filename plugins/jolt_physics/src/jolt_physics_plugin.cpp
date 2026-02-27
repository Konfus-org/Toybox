#include "jolt_physics_plugin.h"
#include "tbx/app/application.h"
#include "tbx/debugging/macros.h"
#include "tbx/graphics/mesh.h"
#include "tbx/graphics/model.h"
#include "tbx/math/transform.h"
#include "tbx/physics/collider.h"
#include "tbx/physics/physics.h"
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/IssueReporting.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/RegisterTypes.h>
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

namespace jolt_physics
{
    static constexpr std::string_view PHYSICS_THREAD_LANE_NAME = "physics";
    static constexpr JPH::ObjectLayer object_layer_static = 0;
    static constexpr JPH::ObjectLayer object_layer_moving = 1;
    static constexpr JPH::BroadPhaseLayer broad_phase_layer_static = JPH::BroadPhaseLayer(0);
    static constexpr JPH::BroadPhaseLayer broad_phase_layer_moving = JPH::BroadPhaseLayer(1);
    static constexpr std::uint32_t broad_phase_layer_count = 2;

    static std::uint32_t get_body_key(const JPH::BodyID& body_id)
    {
        return body_id.GetIndexAndSequenceNumber();
    }

    static bool should_execute_overlap_query(
        ColliderOverlapExecutionMode execution_mode,
        bool is_manual_trigger_requested)
    {
        return execution_mode == ColliderOverlapExecutionMode::AUTO || is_manual_trigger_requested;
    }

    static std::string format_jolt_trace_message(const char* fmt, std::va_list args)
    {
        if (fmt == nullptr || *fmt == '\0')
            return std::string("Jolt reported an empty trace message.");

        std::va_list args_copy;
        va_copy(args_copy, args);
        int required_chars = std::vsnprintf(nullptr, 0, fmt, args_copy);
        va_end(args_copy);
        if (required_chars <= 0)
            return std::string(fmt);

        std::string message = {};
        message.resize(static_cast<std::size_t>(required_chars));
        std::vsnprintf(message.data(), static_cast<std::size_t>(required_chars) + 1U, fmt, args);
        return message;
    }

    static void tbx_jolt_trace_callback(const char* fmt, ...)
    {
        std::va_list args;
        va_start(args, fmt);
        std::string message = format_jolt_trace_message(fmt, args);
        va_end(args);

        TBX_TRACE_INFO("Jolt: {}", message);
    }

#ifdef JPH_ENABLE_ASSERTS
    static bool tbx_jolt_assert_failed_callback(
        const char* expression,
        const char* message,
        const char* file,
        JPH::uint line)
    {
        const char* safe_expression =
            (expression && *expression) ? expression : "<expression unavailable>";
        const char* safe_message = (message && *message) ? message : "";
        const char* safe_file = (file && *file) ? file : "<unknown>";

        if (*safe_message == '\0')
        {
            TBX_TRACE_CRITICAL(
                "Jolt assertion failed: '{}' at {}:{}",
                safe_expression,
                safe_file,
                line);
        }
        else
        {
            TBX_TRACE_CRITICAL(
                "Jolt assertion failed: '{}' at {}:{} ({})",
                safe_expression,
                safe_file,
                line,
                safe_message);
        }

        return false;
    }
#endif

    class PhysicsBroadPhaseLayerInterface final : public JPH::BroadPhaseLayerInterface
    {
      public:
        std::uint32_t GetNumBroadPhaseLayers() const override
        {
            return broad_phase_layer_count;
        }

        JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override
        {
            if (layer == object_layer_moving)
                return broad_phase_layer_moving;

            return broad_phase_layer_static;
        }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override
        {
            if (layer == broad_phase_layer_moving)
                return "Moving";

            return "Static";
        }
#endif
    };

    class PhysicsObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter
    {
      public:
        bool ShouldCollide(JPH::ObjectLayer left, JPH::ObjectLayer right) const override
        {
            if (left == object_layer_static)
                return right == object_layer_moving;

            if (left == object_layer_moving)
                return true;

            return false;
        }
    };

    class PhysicsObjectVsBroadPhaseLayerFilter final : public JPH::ObjectVsBroadPhaseLayerFilter
    {
      public:
        bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer broad_phase_layer)
            const override
        {
            if (layer == object_layer_static)
                return broad_phase_layer == broad_phase_layer_moving;

            if (layer == object_layer_moving)
                return true;

            return false;
        }
    };

    struct JoltRuntimeRef
    {
        std::size_t reference_count = 0;
    };

    static JoltRuntimeRef g_jolt_runtime = {};
    static std::mutex g_jolt_runtime_mutex = {};

    static bool acquire_jolt_runtime()
    {
        auto lock = std::scoped_lock(g_jolt_runtime_mutex);
        if (g_jolt_runtime.reference_count == 0)
        {
            JPH::RegisterDefaultAllocator();
            JPH::Trace = tbx_jolt_trace_callback;
#ifdef JPH_ENABLE_ASSERTS
            JPH::AssertFailed = tbx_jolt_assert_failed_callback;
#endif

            JPH::Factory::sInstance = new JPH::Factory();
            JPH::RegisterTypes();
        }

        ++g_jolt_runtime.reference_count;
        return true;
    }

    static void release_jolt_runtime()
    {
        auto lock = std::scoped_lock(g_jolt_runtime_mutex);
        if (g_jolt_runtime.reference_count == 0)
            return;

        --g_jolt_runtime.reference_count;
        if (g_jolt_runtime.reference_count > 0)
            return;

        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
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

    static float get_vec3_distance_squared(const tbx::Vec3& left, const tbx::Vec3& right)
    {
        const float delta_x = left.x - right.x;
        const float delta_y = left.y - right.y;
        const float delta_z = left.z - right.z;
        return delta_x * delta_x + delta_y * delta_y + delta_z * delta_z;
    }

    static bool has_transform_changed(
        const tbx::Transform& current,
        const tbx::Vec3& previous_position,
        const tbx::Quat& previous_rotation,
        const tbx::Vec3& previous_scale)
    {
        constexpr float position_epsilon_squared = 0.000001F * 0.000001F;
        constexpr float rotation_dot_epsilon = 0.0001F;
        constexpr float scale_epsilon_squared = 0.0001F * 0.0001F;

        if (get_vec3_distance_squared(current.position, previous_position)
            > position_epsilon_squared)
            return true;

        const tbx::Quat current_rotation = normalize(current.rotation);
        const tbx::Quat previous_rotation_normalized = normalize(previous_rotation);
        const float rotation_dot = std::abs(
            current_rotation.x * previous_rotation_normalized.x
            + current_rotation.y * previous_rotation_normalized.y
            + current_rotation.z * previous_rotation_normalized.z
            + current_rotation.w * previous_rotation_normalized.w);
        if ((1.0F - std::min(1.0F, rotation_dot)) > rotation_dot_epsilon)
            return true;

        if (get_vec3_distance_squared(current.scale, previous_scale) > scale_epsilon_squared)
            return true;

        return false;
    }

    static bool has_scale_changed(const tbx::Vec3& current_scale, const tbx::Vec3& previous_scale)
    {
        constexpr float scale_epsilon_squared = 0.0001F * 0.0001F;
        return get_vec3_distance_squared(current_scale, previous_scale) > scale_epsilon_squared;
    }

    static tbx::Vec3 multiply_components(const tbx::Vec3& left, const tbx::Vec3& right)
    {
        return tbx::Vec3(left.x * right.x, left.y * right.y, left.z * right.z);
    }

    static tbx::Vec3 get_safe_normalized(const tbx::Vec3& value, const tbx::Vec3& fallback)
    {
        const float length_squared = value.x * value.x + value.y * value.y + value.z * value.z;
        if (length_squared <= 0.000001F)
            return fallback;

        const float inverse_length = 1.0F / std::sqrt(length_squared);
        return value * inverse_length;
    }

    static tbx::Vec3 calculate_angular_velocity_for_step(
        const tbx::Quat& start_rotation,
        const tbx::Quat& target_rotation,
        float dt_seconds)
    {
        tbx::Quat normalized_start = normalize(start_rotation);
        tbx::Quat normalized_target = normalize(target_rotation);

        tbx::Quat delta_rotation = normalize(normalized_target * glm::conjugate(normalized_start));
        if (delta_rotation.w < 0.0F)
            delta_rotation = -delta_rotation;

        float clamped_w = std::clamp(delta_rotation.w, -1.0F, 1.0F);
        float half_angle_sine = std::sqrt(std::max(0.0F, 1.0F - clamped_w * clamped_w));
        if (half_angle_sine <= 0.000001F)
            return tbx::Vec3(0.0F, 0.0F, 0.0F);

        tbx::Vec3 axis =
            tbx::Vec3(delta_rotation.x, delta_rotation.y, delta_rotation.z) * (1.0F / half_angle_sine);
        float angle_radians = 2.0F * std::atan2(half_angle_sine, clamped_w);
        return axis * (angle_radians / std::max(0.0001F, dt_seconds));
    }

    static tbx::Vec3 get_safe_scale(const tbx::Vec3& scale)
    {
        return tbx::Vec3(
            std::max(0.001F, std::abs(scale.x)),
            std::max(0.001F, std::abs(scale.y)),
            std::max(0.001F, std::abs(scale.z)));
    }

    static JPH::RefConst<JPH::Shape> create_box_shape(const CubeCollider& cube)
    {
        auto half_extents = JPH::Vec3(
            std::max(0.001F, cube.half_extents.x),
            std::max(0.001F, cube.half_extents.y),
            std::max(0.001F, cube.half_extents.z));
        return new JPH::BoxShape(half_extents);
    }

    static JPH::RefConst<JPH::Shape> create_sphere_shape(const SphereCollider& sphere)
    {
        float radius = std::max(0.001F, sphere.radius);
        return new JPH::SphereShape(radius);
    }

    static JPH::RefConst<JPH::Shape> create_capsule_shape(const CapsuleCollider& capsule)
    {
        float radius = std::max(0.001F, capsule.radius);
        float half_height = std::max(0.001F, capsule.half_height);
        return new JPH::CapsuleShape(half_height, radius);
    }

    static void apply_dynamic_body_settings(
        tbx::IPluginHost& host,
        const Physics& physics,
        bool is_trigger_only,
        JPH::BodyCreationSettings& out_body_settings)
    {
        out_body_settings.mIsSensor = is_trigger_only;
        out_body_settings.mAllowSleeping = physics.is_sleep_enabled;
        out_body_settings.mFriction = physics.friction;
        out_body_settings.mRestitution = physics.restitution;
        out_body_settings.mLinearDamping = physics.default_linear_damping;
        out_body_settings.mAngularDamping = physics.default_angular_damping;
        out_body_settings.mLinearVelocity = to_jolt_vec3(physics.linear_velocity);
        out_body_settings.mAngularVelocity = to_jolt_vec3(physics.angular_velocity);
        out_body_settings.mGravityFactor = physics.is_gravity_enabled ? 1.0F : 0.0F;
        out_body_settings.mMaxLinearVelocity =
            std::max(0.0F, host.get_settings().physics.max_linear_velocity.value);
        out_body_settings.mMaxAngularVelocity =
            std::max(0.0F, host.get_settings().physics.max_angular_velocity.value);
        out_body_settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        out_body_settings.mMassPropertiesOverride.mMass = physics.mass;

        constexpr float ccd_linear_speed_threshold = 8.0F;
        const float linear_speed_squared = physics.linear_velocity.x * physics.linear_velocity.x
                                           + physics.linear_velocity.y * physics.linear_velocity.y
                                           + physics.linear_velocity.z * physics.linear_velocity.z;
        const float ccd_threshold_squared = ccd_linear_speed_threshold * ccd_linear_speed_threshold;
        out_body_settings.mMotionQuality = linear_speed_squared >= ccd_threshold_squared
                                               ? JPH::EMotionQuality::LinearCast
                                               : JPH::EMotionQuality::Discrete;
    }

    static bool try_get_mesh_vertex_position_offset(
        const VertexBufferLayout& layout,
        std::size_t& position_offset_bytes)
    {
        for (const auto& attribute : layout.elements)
        {
            if (!std::holds_alternative<tbx::Vec3>(attribute.type))
                continue;

            position_offset_bytes = static_cast<std::size_t>(attribute.offset);
            return true;
        }

        return false;
    }

    static bool try_get_mesh_vertex_positions(
        const tbx::Mesh& mesh,
        const tbx::Vec3& scale,
        std::vector<JPH::Float3>& positions)
    {
        const auto& vertex_values = mesh.vertices.vertices;
        const auto stride_bytes = static_cast<std::size_t>(mesh.vertices.layout.stride);
        if (stride_bytes < sizeof(float) * 3U)
            return false;

        std::size_t position_offset_bytes = 0U;
        if (!try_get_mesh_vertex_position_offset(mesh.vertices.layout, position_offset_bytes))
            position_offset_bytes = 0U;

        if ((stride_bytes % sizeof(float)) != 0U || (position_offset_bytes % sizeof(float)) != 0U)
            return false;

        const std::size_t stride_floats = stride_bytes / sizeof(float);
        const std::size_t position_offset_floats = position_offset_bytes / sizeof(float);
        if (stride_floats == 0U || position_offset_floats + 2U >= stride_floats)
            return false;

        if ((vertex_values.size() % stride_floats) != 0U)
            return false;

        const tbx::Vec3 safe_scale = get_safe_scale(scale);
        const std::size_t vertex_count = vertex_values.size() / stride_floats;
        positions.clear();
        positions.reserve(vertex_count);
        for (std::size_t vertex_index = 0U; vertex_index < vertex_count; ++vertex_index)
        {
            const std::size_t base_index = vertex_index * stride_floats + position_offset_floats;
            positions.push_back(
                JPH::Float3(
                    vertex_values[base_index] * safe_scale.x,
                    vertex_values[base_index + 1U] * safe_scale.y,
                    vertex_values[base_index + 2U] * safe_scale.z));
        }

        return !positions.empty();
    }

    static bool try_append_mesh_geometry(
        const tbx::Mesh& mesh,
        const tbx::Mat4& mesh_transform,
        const tbx::Vec3& mesh_scale,
        std::vector<JPH::Float3>& positions,
        std::vector<JPH::IndexedTriangle>& triangles)
    {
        const auto& vertex_values = mesh.vertices.vertices;
        const auto stride_bytes = static_cast<std::size_t>(mesh.vertices.layout.stride);
        if (stride_bytes < sizeof(float) * 3U)
            return false;

        std::size_t position_offset_bytes = 0U;
        if (!try_get_mesh_vertex_position_offset(mesh.vertices.layout, position_offset_bytes))
            position_offset_bytes = 0U;

        if ((stride_bytes % sizeof(float)) != 0U || (position_offset_bytes % sizeof(float)) != 0U)
            return false;

        const std::size_t stride_floats = stride_bytes / sizeof(float);
        const std::size_t position_offset_floats = position_offset_bytes / sizeof(float);
        if (stride_floats == 0U || position_offset_floats + 2U >= stride_floats)
            return false;

        if ((vertex_values.size() % stride_floats) != 0U)
            return false;

        const std::size_t base_vertex_index = positions.size();
        const tbx::Vec3 safe_scale = get_safe_scale(mesh_scale);
        const std::size_t vertex_count = vertex_values.size() / stride_floats;
        positions.reserve(base_vertex_index + vertex_count);
        for (std::size_t vertex_index = 0U; vertex_index < vertex_count; ++vertex_index)
        {
            const std::size_t base_index = vertex_index * stride_floats + position_offset_floats;
            const tbx::Vec4 local_position = tbx::Vec4(
                vertex_values[base_index],
                vertex_values[base_index + 1U],
                vertex_values[base_index + 2U],
                1.0F);
            const tbx::Vec4 transformed_position = mesh_transform * local_position;

            positions.push_back(
                JPH::Float3(
                    transformed_position.x * safe_scale.x,
                    transformed_position.y * safe_scale.y,
                    transformed_position.z * safe_scale.z));
        }

        const auto& mesh_indices = mesh.indices;
        if (mesh_indices.size() >= 3U)
        {
            const std::size_t triangle_count = mesh_indices.size() / 3U;
            triangles.reserve(triangles.size() + triangle_count);
            for (std::size_t triangle_index = 0U; triangle_index < triangle_count; ++triangle_index)
            {
                const std::size_t index_base = triangle_index * 3U;
                const std::size_t index0 = base_vertex_index + mesh_indices[index_base];
                const std::size_t index1 = base_vertex_index + mesh_indices[index_base + 1U];
                const std::size_t index2 = base_vertex_index + mesh_indices[index_base + 2U];
                if (index0 >= positions.size() || index1 >= positions.size()
                    || index2 >= positions.size())
                    continue;

                triangles.push_back(
                    JPH::IndexedTriangle(
                        static_cast<JPH::uint32>(index0),
                        static_cast<JPH::uint32>(index1),
                        static_cast<JPH::uint32>(index2),
                        0U));
            }
        }

        return positions.size() > base_vertex_index;
    }

    static bool try_get_mesh_collider_data(
        tbx::IPluginHost& host,
        const tbx::Entity& entity,
        const tbx::Vec3& scale,
        std::vector<JPH::Float3>& positions,
        std::vector<JPH::IndexedTriangle>& triangles)
    {
        positions.clear();
        triangles.clear();

        if (entity.has_component<DynamicMesh>())
        {
            const auto& mesh_component = entity.get_component<DynamicMesh>();
            const auto& mesh_data = mesh_component.data;
            if (!mesh_data)
                return false;

            return try_append_mesh_geometry(*mesh_data, tbx::Mat4(1.0F), scale, positions, triangles);
        }

        if (!entity.has_component<StaticMesh>())
            return false;

        const auto& static_mesh = entity.get_component<StaticMesh>();
        if (!static_mesh.handle.is_valid())
            return false;

        auto model = host.get_asset_manager().load<tbx::Model>(static_mesh.handle);
        if (!model || model->meshes.empty())
            return false;

        if (model->parts.empty())
        {
            bool has_any_mesh = false;
            for (const auto& mesh : model->meshes)
                has_any_mesh |=
                    try_append_mesh_geometry(mesh, tbx::Mat4(1.0F), scale, positions, triangles);

            return has_any_mesh;
        }

        auto has_parent = std::vector<bool>(model->parts.size(), false);
        for (const auto& part : model->parts)
        {
            for (const auto child_index : part.children)
            {
                if (child_index < has_parent.size())
                    has_parent[child_index] = true;
            }
        }

        struct PartQueueEntry
        {
            std::size_t part_index = 0U;
            tbx::Mat4 parent_transform = tbx::Mat4(1.0F);
        };

        auto queue = std::vector<PartQueueEntry> {};
        queue.reserve(model->parts.size());
        for (std::size_t part_index = 0U; part_index < model->parts.size(); ++part_index)
        {
            if (has_parent[part_index])
                continue;

            queue.push_back(
                PartQueueEntry {
                    .part_index = part_index,
                    .parent_transform = tbx::Mat4(1.0F),
                });
        }

        if (queue.empty())
        {
            queue.push_back(
                PartQueueEntry {
                    .part_index = 0U,
                    .parent_transform = tbx::Mat4(1.0F),
                });
        }

        auto visited_parts = std::vector<bool>(model->parts.size(), false);
        bool has_any_part_mesh = false;
        while (!queue.empty())
        {
            const PartQueueEntry current = queue.back();
            queue.pop_back();
            if (current.part_index >= model->parts.size())
                continue;

            if (visited_parts[current.part_index])
                continue;
            visited_parts[current.part_index] = true;

            const auto& part = model->parts[current.part_index];
            const tbx::Mat4 part_transform = current.parent_transform * part.transform;
            if (part.mesh_index < model->meshes.size())
            {
                has_any_part_mesh |= try_append_mesh_geometry(
                    model->meshes[part.mesh_index],
                    part_transform,
                    scale,
                    positions,
                    triangles);
            }

            for (const auto child_index : part.children)
            {
                queue.push_back(
                    PartQueueEntry {
                        .part_index = child_index,
                        .parent_transform = part_transform,
                    });
            }
        }

        return has_any_part_mesh;
    }

    static JPH::RefConst<JPH::Shape> create_mesh_shape(
        tbx::IPluginHost& host,
        const tbx::Entity& entity,
        const tbx::Transform& transform,
        bool is_physics_driven)
    {
        const auto& mesh_collider = entity.get_component<MeshCollider>();

        auto positions = std::vector<JPH::Float3> {};
        auto triangles = std::vector<JPH::IndexedTriangle> {};
        if (!try_get_mesh_collider_data(host, entity, transform.scale, positions, triangles))
        {
            TBX_TRACE_WARNING(
                "Jolt physics: MeshCollider on entity {} has no usable mesh geometry, "
                "using fallback box shape.",
                to_string(entity.get_id()));
            return nullptr;
        }

        if (mesh_collider.is_convex || is_physics_driven)
        {
            JPH::Array<JPH::Vec3> convex_points = {};
            convex_points.reserve(static_cast<JPH::uint>(positions.size()));
            for (const auto& point : positions)
                convex_points.push_back(JPH::Vec3(point.x, point.y, point.z));

            auto convex_shape_result = JPH::ConvexHullShapeSettings(convex_points).Create();
            if (convex_shape_result.HasError())
            {
                TBX_TRACE_WARNING(
                    "Jolt physics: Failed to build convex MeshCollider on entity {}: {}",
                    to_string(entity.get_id()),
                    convex_shape_result.GetError().c_str());
                return nullptr;
            }

            return convex_shape_result.Get();
        }

        if (triangles.empty())
        {
            TBX_TRACE_WARNING(
                "Jolt physics: Non-convex MeshCollider on entity {} has no triangle index "
                "data.",
                to_string(entity.get_id()));
            return nullptr;
        }

        JPH::VertexList vertex_list = {};
        vertex_list.reserve(static_cast<JPH::uint>(positions.size()));
        for (const auto& position : positions)
            vertex_list.push_back(position);

        JPH::IndexedTriangleList triangle_list = {};
        triangle_list.reserve(static_cast<JPH::uint>(triangles.size()));
        for (const auto& triangle : triangles)
            triangle_list.push_back(triangle);

        auto mesh_shape_result =
            JPH::MeshShapeSettings(std::move(vertex_list), std::move(triangle_list)).Create();
        if (mesh_shape_result.HasError())
        {
            TBX_TRACE_WARNING(
                "Jolt physics: Failed to build mesh MeshCollider on entity {}: {}",
                to_string(entity.get_id()),
                mesh_shape_result.GetError().c_str());
            return nullptr;
        }

        return mesh_shape_result.Get();
    }

    static bool has_any_collider(const tbx::Entity& entity)
    {
        return entity.has_component<SphereCollider>() || entity.has_component<CapsuleCollider>()
               || entity.has_component<CubeCollider>() || entity.has_component<MeshCollider>();
    }

    static const ColliderTrigger* try_get_trigger_collider(const tbx::Entity& entity)
    {
        if (entity.has_component<SphereCollider>())
            return &entity.get_component<SphereCollider>().trigger;

        if (entity.has_component<CapsuleCollider>())
            return &entity.get_component<CapsuleCollider>().trigger;

        if (entity.has_component<CubeCollider>())
            return &entity.get_component<CubeCollider>().trigger;

        if (entity.has_component<MeshCollider>())
            return &entity.get_component<MeshCollider>().trigger;

        return nullptr;
    }

    static ColliderTrigger* try_get_trigger_collider(tbx::Entity& entity)
    {
        if (entity.has_component<SphereCollider>())
            return &entity.get_component<SphereCollider>().trigger;

        if (entity.has_component<CapsuleCollider>())
            return &entity.get_component<CapsuleCollider>().trigger;

        if (entity.has_component<CubeCollider>())
            return &entity.get_component<CubeCollider>().trigger;

        if (entity.has_component<MeshCollider>())
            return &entity.get_component<MeshCollider>().trigger;

        return nullptr;
    }

    static bool is_trigger_only_collider(const tbx::Entity& entity)
    {
        const ColliderTrigger* trigger = try_get_trigger_collider(entity);
        if (trigger == nullptr)
            return false;

        return trigger->is_trigger_only;
    }

    static JPH::RefConst<JPH::Shape> create_shape_for_entity(
        tbx::IPluginHost& host,
        const tbx::Entity& entity,
        const tbx::Transform& transform,
        bool is_physics_driven)
    {
        if (entity.has_component<SphereCollider>())
            return create_sphere_shape(entity.get_component<SphereCollider>());

        if (entity.has_component<CapsuleCollider>())
            return create_capsule_shape(entity.get_component<CapsuleCollider>());

        if (entity.has_component<CubeCollider>())
            return create_box_shape(entity.get_component<CubeCollider>());

        if (entity.has_component<MeshCollider>())
        {
            JPH::RefConst<JPH::Shape> mesh_shape =
                create_mesh_shape(host, entity, transform, is_physics_driven);
            if (mesh_shape)
                return mesh_shape;

            return new JPH::BoxShape(JPH::Vec3(0.5F, 0.5F, 0.5F));
        }

        if (!entity.has_component<Physics>())
            return nullptr;

        return new JPH::BoxShape(JPH::Vec3(0.5F, 0.5F, 0.5F));
    }

    static JPH::EMotionType get_motion_type(const Physics& physics)
    {
        if (physics.is_kinematic)
            return JPH::EMotionType::Kinematic;

        return JPH::EMotionType::Dynamic;
    }

    JoltPhysicsPlugin::~JoltPhysicsPlugin() = default;

    void JoltPhysicsPlugin::on_attach(tbx::IPluginHost&)
    {
        auto& thread_manager = get_host().get_thread_manager();
        thread_manager.try_create_lane(PHYSICS_THREAD_LANE_NAME);
        if (thread_manager.has_lane(PHYSICS_THREAD_LANE_NAME))
        {
            _physics_thread_id = thread_manager
                                     .post_with_future(
                                         PHYSICS_THREAD_LANE_NAME,
                                         []()
                                         {
                                             return std::this_thread::get_id();
                                         })
                                     .get();
        }

        run_on_physics_lane_and_wait(
            [this]()
            {
                if (!acquire_jolt_runtime())
                {
                    TBX_TRACE_ERROR("Jolt physics: failed to initialize Jolt runtime.");
                    return;
                }

                constexpr JPH::uint temp_allocator_bytes = 64U * 1024U * 1024U;
                _temp_allocator = std::make_unique<JPH::TempAllocatorImplWithMallocFallback>(
                    temp_allocator_bytes);
                _job_system = std::make_unique<JPH::JobSystemThreadPool>(
                    JPH::cMaxPhysicsJobs,
                    JPH::cMaxPhysicsBarriers);

                auto& settings = get_host().get_settings().physics;

                auto max_bodies = std::max<std::uint32_t>(1U, settings.max_body_count.value);
                auto max_pairs = std::max<std::uint32_t>(1U, settings.max_body_pairs.value);
                auto max_constraints =
                    std::max<std::uint32_t>(1U, settings.max_contact_constraints.value);

                static auto broad_phase_interface = PhysicsBroadPhaseLayerInterface();
                static auto object_vs_broad_phase_filter = PhysicsObjectVsBroadPhaseLayerFilter();
                static auto object_layer_pair_filter = PhysicsObjectLayerPairFilter();

                _physics_system.Init(
                    max_bodies,
                    0,
                    max_pairs,
                    max_constraints,
                    broad_phase_interface,
                    object_vs_broad_phase_filter,
                    object_layer_pair_filter);

                apply_world_settings();
                _is_ready = true;
            });
    }

    void JoltPhysicsPlugin::on_detach()
    {
        run_on_physics_lane_and_wait(
            [this]()
            {
                clear_bodies();
                _job_system.reset();
                _temp_allocator.reset();
                _is_ready = false;
                release_jolt_runtime();
            });
        _physics_thread_id = {};
    }

    void JoltPhysicsPlugin::on_fixed_update(const tbx::DeltaTime& dt)
    {
        run_on_physics_lane_and_wait(
            [this, dt]()
            {
                if (!_is_ready || !_temp_allocator || !_job_system)
                    return;

                apply_world_settings();
                sync_entities_to_world(static_cast<float>(dt.seconds));

                JPH::EPhysicsUpdateError update_error = _physics_system.Update(
                    static_cast<float>(std::max(0.0001, dt.seconds)),
                    1,
                    _temp_allocator.get(),
                    _job_system.get());
                if (update_error != JPH::EPhysicsUpdateError::None)
                    TBX_TRACE_WARNING(
                        "Jolt physics update reported error flags: {}",
                        static_cast<std::uint32_t>(update_error));

                sync_world_to_entities();
                process_trigger_colliders();
            });
    }

    void JoltPhysicsPlugin::on_recieve_message(tbx::Message& msg)
    {
        if (auto* raycast_request = handle_message<RaycastRequest>(msg))
        {
            run_on_physics_lane_and_wait(
                [this, raycast_request]()
                {
                    handle_raycast_request(*raycast_request);
                });
            return;
        }
    }

    bool JoltPhysicsPlugin::is_on_physics_thread() const
    {
        return _physics_thread_id != std::thread::id {}
               && std::this_thread::get_id() == _physics_thread_id;
    }

    void JoltPhysicsPlugin::run_on_physics_lane_and_wait(const std::function<void()>& work)
    {
        if (!work)
            return;

        auto& thread_manager = get_host().get_thread_manager();
        if (!thread_manager.has_lane(PHYSICS_THREAD_LANE_NAME) || is_on_physics_thread())
        {
            work();
            return;
        }

        thread_manager.post_with_future(PHYSICS_THREAD_LANE_NAME, work).get();
    }

    void JoltPhysicsPlugin::clear_bodies()
    {
        auto& body_interface = _physics_system.GetBodyInterface();

        for (const auto& body_entry : _bodies_by_entity)
        {
            const JPH::BodyID& body_id = body_entry.second.body_id;
            if (!body_interface.IsAdded(body_id))
                continue;

            body_interface.RemoveBody(body_id);
            body_interface.DestroyBody(body_id);
        }

        _bodies_by_entity.clear();
        _entity_by_body_key.clear();
        _overlap_entities_by_trigger.clear();
    }

    void JoltPhysicsPlugin::apply_world_settings()
    {
        auto& settings = get_host().get_settings().physics;

        auto jolt_settings = _physics_system.GetPhysicsSettings();
        jolt_settings.mNumVelocitySteps =
            std::max<std::uint32_t>(2U, settings.solver_velocity_iterations.value);
        jolt_settings.mNumPositionSteps =
            std::max<std::uint32_t>(1U, settings.solver_position_iterations.value);

        _physics_system.SetPhysicsSettings(jolt_settings);
        _physics_system.SetGravity(to_jolt_vec3(settings.gravity.value));
    }

    void JoltPhysicsPlugin::sync_entities_to_world(float dt_seconds)
    {
        auto& registry = get_host().get_entity_registry();
        auto active_entities = std::unordered_set<Uuid>();

        auto entities = registry.get_with<tbx::Transform>();
        for (auto& entity : entities)
        {
            Uuid entity_id = entity.get_id();

            const auto world_transform = get_world_space_transform(entity);
            const bool has_physics_component = entity.has_component<Physics>();
            const bool has_collider = has_any_collider(entity);
            if (!has_physics_component && !has_collider)
                continue;

            const bool is_trigger_only = is_trigger_only_collider(entity);

            const auto* physics =
                has_physics_component ? &entity.get_component<Physics>() : nullptr;
            const bool is_physics_driven = physics != nullptr && physics->is_valid();
            if (has_physics_component && !is_physics_driven)
                continue;

            active_entities.insert(entity_id);

            auto body_it = _bodies_by_entity.find(entity_id);
            auto& body_interface = _physics_system.GetBodyInterface();

            if (body_it != _bodies_by_entity.end()
                && body_it->second.is_physics_driven != is_physics_driven)
            {
                const JPH::BodyID existing_body_id = body_it->second.body_id;
                if (body_interface.IsAdded(existing_body_id))
                {
                    body_interface.RemoveBody(existing_body_id);
                    body_interface.DestroyBody(existing_body_id);
                }

                _entity_by_body_key.erase(get_body_key(existing_body_id));
                _bodies_by_entity.erase(body_it);
                body_it = _bodies_by_entity.end();
            }
            else if (
                body_it != _bodies_by_entity.end()
                && body_it->second.is_trigger_only != is_trigger_only)
            {
                const JPH::BodyID existing_body_id = body_it->second.body_id;
                if (body_interface.IsAdded(existing_body_id))
                {
                    body_interface.RemoveBody(existing_body_id);
                    body_interface.DestroyBody(existing_body_id);
                }

                _entity_by_body_key.erase(get_body_key(existing_body_id));
                _bodies_by_entity.erase(body_it);
                body_it = _bodies_by_entity.end();
            }
            else if (
                body_it != _bodies_by_entity.end() && entity.has_component<MeshCollider>()
                && body_it->second.has_last_transform
                && has_scale_changed(world_transform.scale, body_it->second.last_scale))
            {
                const JPH::BodyID existing_body_id = body_it->second.body_id;
                if (body_interface.IsAdded(existing_body_id))
                {
                    body_interface.RemoveBody(existing_body_id);
                    body_interface.DestroyBody(existing_body_id);
                }

                _entity_by_body_key.erase(get_body_key(existing_body_id));
                _bodies_by_entity.erase(body_it);
                body_it = _bodies_by_entity.end();
            }

            if (body_it == _bodies_by_entity.end())
            {
                JPH::RefConst<JPH::Shape> shape =
                    create_shape_for_entity(get_host(), entity, world_transform, is_physics_driven);
                if (!shape)
                    continue;

                auto object_layer = is_physics_driven ? object_layer_moving : object_layer_static;
                auto motion_type =
                    is_physics_driven ? get_motion_type(*physics) : JPH::EMotionType::Static;

                auto body_settings = JPH::BodyCreationSettings(
                    shape,
                    to_jolt_rvec3(world_transform.position),
                    to_jolt_quat(world_transform.rotation),
                    motion_type,
                    object_layer);
                body_settings.mIsSensor = is_trigger_only;

                if (is_physics_driven)
                {
                    apply_dynamic_body_settings(
                        get_host(),
                        *physics,
                        is_trigger_only,
                        body_settings);
                }

                auto activation = JPH::EActivation::DontActivate;
                if (is_physics_driven)
                    activation = physics->is_kinematic ? JPH::EActivation::DontActivate
                                                       : JPH::EActivation::Activate;

                JPH::BodyID body_id =
                    _physics_system.GetBodyInterface().CreateAndAddBody(body_settings, activation);
                if (body_id.IsInvalid())
                {
                    TBX_TRACE_WARNING(
                        "Jolt physics: failed to create a body for entity {}.",
                        to_string(entity_id));
                    continue;
                }

                _bodies_by_entity[entity_id] = BodyRecord {
                    .body_id = body_id,
                    .is_physics_driven = is_physics_driven,
                    .last_position = world_transform.position,
                    .last_rotation = world_transform.rotation,
                    .last_scale = world_transform.scale,
                    .has_last_transform = true,
                    .is_trigger_only = is_trigger_only,
                };
                _entity_by_body_key[get_body_key(body_id)] = entity_id;
                continue;
            }

            auto& body_record = body_it->second;
            body_record.is_trigger_only = is_trigger_only;
            JPH::BodyID body_id = body_record.body_id;
            if (!body_interface.IsAdded(body_id))
                continue;

            const bool transform_is_dirty = body_record.has_last_transform
                                            && has_transform_changed(
                                                world_transform,
                                                body_record.last_position,
                                                body_record.last_rotation,
                                                body_record.last_scale);

            if (!is_physics_driven)
            {
                body_interface.SetPositionAndRotation(
                    body_id,
                    to_jolt_rvec3(world_transform.position),
                    to_jolt_quat(world_transform.rotation),
                    JPH::EActivation::DontActivate);
                body_record.last_position = world_transform.position;
                body_record.last_rotation = world_transform.rotation;
                body_record.last_scale = world_transform.scale;
                body_record.has_last_transform = true;
                continue;
            }

            if (physics->is_kinematic)
            {
                body_interface.MoveKinematic(
                    body_id,
                    to_jolt_rvec3(world_transform.position),
                    to_jolt_quat(world_transform.rotation),
                    std::max(0.0001F, dt_seconds));

                body_interface.SetLinearVelocity(body_id, to_jolt_vec3(physics->linear_velocity));
                body_interface.SetAngularVelocity(body_id, to_jolt_vec3(physics->angular_velocity));
                body_record.last_position = world_transform.position;
                body_record.last_rotation = world_transform.rotation;
                body_record.last_scale = world_transform.scale;
                body_record.has_last_transform = true;
            }
            else if (transform_is_dirty)
            {
                if (physics->transform_sync_mode == PhysicsTransformSyncMode::TELEPORT)
                {
                    body_interface.SetPositionAndRotation(
                        body_id,
                        to_jolt_rvec3(world_transform.position),
                        to_jolt_quat(world_transform.rotation),
                        JPH::EActivation::Activate);
                }
                else if (physics->transform_sync_mode == PhysicsTransformSyncMode::SWEEP)
                {
                    const tbx::Vec3 current_position =
                        to_tbx_vec3_from_rvec3(body_interface.GetPosition(body_id));
                    const tbx::Quat current_rotation = to_tbx_quat(body_interface.GetRotation(body_id));
                    const float safe_dt_seconds = std::max(0.0001F, dt_seconds);

                    tbx::Vec3 linear_velocity =
                        (world_transform.position - current_position) / safe_dt_seconds;
                    tbx::Vec3 angular_velocity = calculate_angular_velocity_for_step(
                        current_rotation,
                        world_transform.rotation,
                        safe_dt_seconds);

                    body_interface.SetLinearVelocity(body_id, to_jolt_vec3(linear_velocity));
                    body_interface.SetAngularVelocity(body_id, to_jolt_vec3(angular_velocity));
                    body_interface.ActivateBody(body_id);
                }
            }

            body_interface.SetFriction(body_id, physics->friction);
            body_interface.SetRestitution(body_id, physics->restitution);
            body_interface.SetGravityFactor(body_id, physics->is_gravity_enabled ? 1.0F : 0.0F);
        }

        auto stale_entities = std::vector<Uuid>();
        stale_entities.reserve(_bodies_by_entity.size());
        for (const auto& body_entry : _bodies_by_entity)
        {
            if (active_entities.contains(body_entry.first))
                continue;

            stale_entities.push_back(body_entry.first);
        }

        auto& body_interface = _physics_system.GetBodyInterface();
        for (const Uuid& stale_entity : stale_entities)
        {
            auto body_it = _bodies_by_entity.find(stale_entity);
            if (body_it == _bodies_by_entity.end())
                continue;

            const JPH::BodyID body_id = body_it->second.body_id;
            if (body_interface.IsAdded(body_id))
            {
                body_interface.RemoveBody(body_id);
                body_interface.DestroyBody(body_id);
            }

            _entity_by_body_key.erase(get_body_key(body_id));
            _bodies_by_entity.erase(body_it);
        }
    }

    void JoltPhysicsPlugin::sync_world_to_entities()
    {
        auto& registry = get_host().get_entity_registry();
        auto& body_interface = _physics_system.GetBodyInterface();

        for (auto& body_entry : _bodies_by_entity)
        {
            const Uuid& entity_id = body_entry.first;
            auto& body_record = body_entry.second;
            const JPH::BodyID& body_id = body_record.body_id;

            if (!registry.has<tbx::Transform>(entity_id))
                continue;

            auto& transform = registry.get_with<tbx::Transform>(entity_id);
            auto entity = registry.get(entity_id);
            if (!entity.get_id().is_valid())
                continue;

            if (!body_interface.IsAdded(body_id))
                continue;

            if (!registry.has<Physics>(entity_id))
            {
                const auto world_transform = get_world_space_transform(entity);
                body_record.last_position = world_transform.position;
                body_record.last_rotation = world_transform.rotation;
                body_record.last_scale = world_transform.scale;
                body_record.has_last_transform = true;
                continue;
            }

            auto& physics = registry.get_with<Physics>(entity_id);
            physics.linear_velocity = to_tbx_vec3(body_interface.GetLinearVelocity(body_id));
            physics.angular_velocity = to_tbx_vec3(body_interface.GetAngularVelocity(body_id));

            if (physics.is_kinematic)
            {
                const auto world_transform = get_world_space_transform(entity);
                body_record.last_position = world_transform.position;
                body_record.last_rotation = world_transform.rotation;
                body_record.last_scale = world_transform.scale;
                body_record.has_last_transform = true;
                continue;
            }

            auto world_transform = tbx::Transform {};
            world_transform.position = to_tbx_vec3_from_rvec3(body_interface.GetPosition(body_id));
            world_transform.rotation = to_tbx_quat(body_interface.GetRotation(body_id));
            world_transform.scale = transform.scale;

            auto parent_entity = tbx::Entity {};
            if (entity.try_get_parent_entity(parent_entity))
            {
                const auto parent_world_transform = get_world_space_transform(parent_entity);
                const auto local_transform =
                    world_to_local_tranform(parent_world_transform, world_transform);
                transform.position = local_transform.position;
                transform.rotation = local_transform.rotation;
            }
            else
            {
                transform.position = world_transform.position;
                transform.rotation = world_transform.rotation;
            }

            body_record.last_position = world_transform.position;
            body_record.last_rotation = world_transform.rotation;
            body_record.last_scale = world_transform.scale;
            body_record.has_last_transform = true;
        }
    }

    Uuid JoltPhysicsPlugin::try_get_entity_for_body(const JPH::BodyID& body_id) const
    {
        auto entity_it = _entity_by_body_key.find(get_body_key(body_id));
        if (entity_it == _entity_by_body_key.end())
            return {};

        return entity_it->second;
    }

    void JoltPhysicsPlugin::process_trigger_colliders()
    {
        auto& registry = get_host().get_entity_registry();
        auto& body_interface = _physics_system.GetBodyInterface();
        const auto& narrow_phase_query = _physics_system.GetNarrowPhaseQuery();

        auto active_trigger_entities = std::unordered_set<Uuid>();
        auto trigger_entities = registry.get_with<tbx::Transform>();
        for (auto& trigger_entity : trigger_entities)
        {
            const Uuid trigger_entity_id = trigger_entity.get_id();
            auto* trigger_collider = try_get_trigger_collider(trigger_entity);
            if (trigger_collider == nullptr)
                continue;

            active_trigger_entities.insert(trigger_entity_id);

            if (!trigger_collider->is_overlap_enabled)
            {
                trigger_collider->is_manual_scan_requested = false;

                auto previous_overlaps_it = _overlap_entities_by_trigger.find(trigger_entity_id);
                if (previous_overlaps_it != _overlap_entities_by_trigger.end())
                {
                    for (const Uuid& overlapped_entity_id : previous_overlaps_it->second)
                    {
                        const ColliderOverlapEvent event = ColliderOverlapEvent {
                            .trigger_entity_id = trigger_entity_id,
                            .overlapped_entity_id = overlapped_entity_id,
                        };
                        for (const auto& callback : trigger_collider->overlap_end_callbacks)
                        {
                            if (callback)
                                callback(event);
                        }
                    }
                }

                _overlap_entities_by_trigger.erase(trigger_entity_id);
                continue;
            }

            if (!should_execute_overlap_query(
                    trigger_collider->overlap_execution_mode,
                    trigger_collider->is_manual_scan_requested))
            {
                trigger_collider->is_manual_scan_requested = false;
                continue;
            }
            trigger_collider->is_manual_scan_requested = false;

            auto current_overlaps = std::unordered_set<Uuid>();

            auto body_it = _bodies_by_entity.find(trigger_entity_id);
            if (body_it != _bodies_by_entity.end()
                && body_interface.IsAdded(body_it->second.body_id))
            {
                const JPH::BodyID trigger_body_id = body_it->second.body_id;
                JPH::RefConst<JPH::Shape> trigger_shape = body_interface.GetShape(trigger_body_id);
                if (trigger_shape)
                {
                    JPH::CollideShapeSettings collide_settings = {};
                    collide_settings.mBackFaceMode = JPH::EBackFaceMode::CollideWithBackFaces;

                    JPH::IgnoreSingleBodyFilter ignore_trigger_body_filter =
                        JPH::IgnoreSingleBodyFilter(trigger_body_id);
                    JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector = {};

                    narrow_phase_query.CollideShape(
                        trigger_shape.GetPtr(),
                        JPH::Vec3::sReplicate(1.0F),
                        body_interface.GetCenterOfMassTransform(trigger_body_id),
                        collide_settings,
                        JPH::RVec3::sZero(),
                        collector,
                        {},
                        {},
                        ignore_trigger_body_filter);

                    current_overlaps.reserve(static_cast<std::size_t>(collector.mHits.size()));
                    for (const auto& overlap_hit : collector.mHits)
                    {
                        const Uuid overlapped_entity_id =
                            try_get_entity_for_body(overlap_hit.mBodyID2);
                        if (!overlapped_entity_id.is_valid()
                            || overlapped_entity_id == trigger_entity_id)
                            continue;

                        current_overlaps.insert(overlapped_entity_id);
                    }
                }
            }

            auto& previous_overlaps = _overlap_entities_by_trigger[trigger_entity_id];
            for (const Uuid& overlapped_entity_id : current_overlaps)
            {
                const ColliderOverlapEvent event = ColliderOverlapEvent {
                    .trigger_entity_id = trigger_entity_id,
                    .overlapped_entity_id = overlapped_entity_id,
                };
                const bool was_overlapping = previous_overlaps.contains(overlapped_entity_id);
                const auto& callbacks = was_overlapping ? trigger_collider->overlap_stay_callbacks
                                                        : trigger_collider->overlap_begin_callbacks;
                for (const auto& callback : callbacks)
                {
                    if (callback)
                        callback(event);
                }
            }

            for (const Uuid& overlapped_entity_id : previous_overlaps)
            {
                if (current_overlaps.contains(overlapped_entity_id))
                    continue;

                const ColliderOverlapEvent event = ColliderOverlapEvent {
                    .trigger_entity_id = trigger_entity_id,
                    .overlapped_entity_id = overlapped_entity_id,
                };
                for (const auto& callback : trigger_collider->overlap_end_callbacks)
                {
                    if (callback)
                        callback(event);
                }
            }

            if (current_overlaps.empty())
            {
                _overlap_entities_by_trigger.erase(trigger_entity_id);
                continue;
            }

            previous_overlaps = std::move(current_overlaps);
        }

        auto stale_trigger_entities = std::vector<Uuid>();
        stale_trigger_entities.reserve(_overlap_entities_by_trigger.size());
        for (const auto& overlap_entry : _overlap_entities_by_trigger)
        {
            if (active_trigger_entities.contains(overlap_entry.first))
                continue;

            stale_trigger_entities.push_back(overlap_entry.first);
        }

        for (const Uuid& stale_trigger_entity : stale_trigger_entities)
            _overlap_entities_by_trigger.erase(stale_trigger_entity);
    }

    void JoltPhysicsPlugin::handle_raycast_request(RaycastRequest& request) const
    {
        request.result = RaycastResult {};
        if (!_is_ready)
        {
            request.state = MessageState::ERROR;
            request.Message::result.flag_failure("Physics backend is not initialized.");
            return;
        }

        const auto& body_interface = _physics_system.GetBodyInterface();
        const auto& narrow_phase_query = _physics_system.GetNarrowPhaseQuery();

        const tbx::Vec3 ray_direction =
            get_safe_normalized(request.raycast.direction, tbx::Vec3(0.0F, 0.0F, -1.0F));
        const float max_distance = std::max(0.0F, request.raycast.max_distance);
        if (max_distance > 0.0F)
        {
            const JPH::RRayCast ray = JPH::RRayCast(
                to_jolt_rvec3(request.raycast.origin),
                to_jolt_vec3(ray_direction * max_distance));

            JPH::RayCastResult ray_hit = {};
            bool has_hit = false;
            if (request.raycast.ignore_entity && request.raycast.ignored_entity_id.is_valid())
            {
                auto body_it = _bodies_by_entity.find(request.raycast.ignored_entity_id);
                if (body_it != _bodies_by_entity.end()
                    && body_interface.IsAdded(body_it->second.body_id))
                {
                    JPH::IgnoreSingleBodyFilter ignore_body_filter =
                        JPH::IgnoreSingleBodyFilter(body_it->second.body_id);
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

            request.result.has_hit = has_hit;
            if (has_hit)
            {
                request.result.hit_entity_id = try_get_entity_for_body(ray_hit.mBodyID);
                request.result.hit_fraction = ray_hit.mFraction;
                request.result.hit_position =
                    to_tbx_vec3_from_rvec3(ray.GetPointOnRay(ray_hit.mFraction));
            }
        }

        request.state = MessageState::HANDLED;
        request.Message::result.flag_success();
    }
}
