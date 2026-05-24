#pragma once
#include "tbx/systems/app/settings.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/physics/physics.h"
#include "tbx/types/components/collider.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/components/model.h"
#include "tbx/types/components/rigidbody.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/quaternions.h"
#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace tbx::internal
{
    static bool has_any_collider(const Entity& entity)
    {
        return entity.has_component<SphereCollider>() || entity.has_component<CapsuleCollider>()
               || entity.has_component<CubeCollider>() || entity.has_component<MeshCollider>();
    }

    static const ColliderTrigger* try_get_trigger_collider(const Entity& entity)
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

    static ColliderTrigger* try_get_trigger_collider(Entity& entity)
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

    static bool is_trigger_only_collider(const Entity& entity)
    {
        const ColliderTrigger* trigger = try_get_trigger_collider(entity);
        if (trigger == nullptr)
            return false;

        return trigger->is_trigger_only;
    }

    static bool should_execute_overlap_query(
        ColliderOverlapExecutionMode execution_mode,
        bool is_manual_trigger_requested)
    {
        return execution_mode == ColliderOverlapExecutionMode::AUTO || is_manual_trigger_requested;
    }

    static float get_vec3_distance_squared(const Vec3& left, const Vec3& right)
    {
        const float delta_x = left.x - right.x;
        const float delta_y = left.y - right.y;
        const float delta_z = left.z - right.z;
        return delta_x * delta_x + delta_y * delta_y + delta_z * delta_z;
    }

    static bool has_scale_changed(const Vec3& current_scale, const Vec3& previous_scale)
    {
        constexpr float scale_epsilon_squared = 0.0001F * 0.0001F;
        return get_vec3_distance_squared(current_scale, previous_scale) > scale_epsilon_squared;
    }

    static bool has_transform_changed(
        const Transform& current,
        const Vec3& previous_position,
        const Quat& previous_rotation,
        const Vec3& previous_scale)
    {
        constexpr float position_epsilon_squared = 0.000001F * 0.000001F;
        constexpr float rotation_dot_epsilon = 0.0001F;
        constexpr float scale_epsilon_squared = 0.0001F * 0.0001F;

        if (get_vec3_distance_squared(current.position, previous_position)
            > position_epsilon_squared)
            return true;

        const Quat current_rotation = normalize(current.rotation);
        const Quat previous_rotation_normalized = normalize(previous_rotation);
        const float rotation_dot = std::abs(
            current_rotation.x * previous_rotation_normalized.x
            + current_rotation.y * previous_rotation_normalized.y
            + current_rotation.z * previous_rotation_normalized.z
            + current_rotation.w * previous_rotation_normalized.w);
        if ((1.0F - std::min(1.0F, rotation_dot)) > rotation_dot_epsilon)
            return true;

        return get_vec3_distance_squared(current.scale, previous_scale) > scale_epsilon_squared;
    }

    static Vec3 calculate_angular_velocity_for_step(
        const Quat& start_rotation,
        const Quat& target_rotation,
        float dt_seconds)
    {
        Quat normalized_start = normalize(start_rotation);
        Quat normalized_target = normalize(target_rotation);

        Quat delta_rotation = normalize(normalized_target * glm::conjugate(normalized_start));
        if (delta_rotation.w < 0.0F)
            delta_rotation = -delta_rotation;

        float clamped_w = std::clamp(delta_rotation.w, -1.0F, 1.0F);
        float half_angle_sine = std::sqrt(std::max(0.0F, 1.0F - clamped_w * clamped_w));
        if (half_angle_sine <= 0.000001F)
            return Vec3(0.0F, 0.0F, 0.0F);

        Vec3 axis =
            Vec3(delta_rotation.x, delta_rotation.y, delta_rotation.z) * (1.0F / half_angle_sine);
        float angle_radians = 2.0F * std::atan2(half_angle_sine, clamped_w);
        return axis * (angle_radians / std::max(0.0001F, dt_seconds));
    }

    static Vec3 get_safe_scale(const Vec3& scale)
    {
        return Vec3(
            std::max(0.001F, std::abs(scale.x)),
            std::max(0.001F, std::abs(scale.y)),
            std::max(0.001F, std::abs(scale.z)));
    }

    static bool try_get_mesh_vertex_position_offset(
        const VertexBufferLayout& layout,
        size& position_offset_bytes)
    {
        for (const auto& attribute : layout.elements)
        {
            if (attribute.type != GraphicsVertexFormat::VEC3)
                continue;

            position_offset_bytes = static_cast<size>(attribute.offset);
            return true;
        }

        return false;
    }

    static bool try_append_mesh_geometry(
        const Mesh& mesh,
        const Mat4& mesh_transform,
        const Vec3& mesh_scale,
        std::vector<Vec3>& vertices,
        std::vector<PhysicsMeshTriangle>& triangles)
    {
        const auto& vertex_values = mesh.vertices.vertices;
        const size stride_bytes = static_cast<size>(mesh.vertices.layout.stride);
        if (stride_bytes < sizeof(float) * 3U)
            return false;

        size position_offset_bytes = 0U;
        if (!try_get_mesh_vertex_position_offset(mesh.vertices.layout, position_offset_bytes))
            position_offset_bytes = 0U;

        if ((stride_bytes % sizeof(float)) != 0U || (position_offset_bytes % sizeof(float)) != 0U)
            return false;

        const size stride_floats = stride_bytes / sizeof(float);
        const size position_offset_floats = position_offset_bytes / sizeof(float);
        if (stride_floats == 0U || position_offset_floats + 2U >= stride_floats)
            return false;

        if ((vertex_values.size() % stride_floats) != 0U)
            return false;

        const size base_vertex_index = static_cast<size>(vertices.size());
        const Vec3 safe_scale = get_safe_scale(mesh_scale);
        const size vertex_count = static_cast<size>(vertex_values.size()) / stride_floats;
        vertices.reserve(base_vertex_index + vertex_count);
        for (size vertex_index = 0U; vertex_index < vertex_count; ++vertex_index)
        {
            const size base_index = vertex_index * stride_floats + position_offset_floats;
            const Vec4 local_position = Vec4(
                vertex_values[base_index],
                vertex_values[base_index + 1U],
                vertex_values[base_index + 2U],
                1.0F);
            const Vec4 transformed_position = mesh_transform * local_position;

            vertices.push_back(Vec3(
                transformed_position.x * safe_scale.x,
                transformed_position.y * safe_scale.y,
                transformed_position.z * safe_scale.z));
        }

        const auto& mesh_indices = mesh.indices;
        if (mesh_indices.size() >= 3U)
        {
            const size triangle_count = static_cast<size>(mesh_indices.size()) / 3U;
            triangles.reserve(triangles.size() + triangle_count);
            for (size triangle_index = 0U; triangle_index < triangle_count; ++triangle_index)
            {
                const size index_base = triangle_index * 3U;
                const size index0 = base_vertex_index + mesh_indices[index_base];
                const size index1 = base_vertex_index + mesh_indices[index_base + 1U];
                const size index2 = base_vertex_index + mesh_indices[index_base + 2U];
                if (index0 >= vertices.size() || index1 >= vertices.size()
                    || index2 >= vertices.size())
                    continue;

                triangles.push_back(
                    PhysicsMeshTriangle {
                        .index0 = static_cast<uint32>(index0),
                        .index1 = static_cast<uint32>(index1),
                        .index2 = static_cast<uint32>(index2),
                    });
            }
        }

        return vertices.size() > base_vertex_index;
    }

    struct MeshPartQueueEntry
    {
        size part_index = 0U;
        Mat4 parent_transform = Mat4(1.0F);
    };

    static bool try_get_mesh_collider_data(
        AssetManager& asset_manager,
        const Entity& entity,
        const Vec3& scale,
        std::vector<Vec3>& vertices,
        std::vector<PhysicsMeshTriangle>& triangles)
    {
        vertices.clear();
        triangles.clear();

        if (entity.has_component<DynamicMesh>())
        {
            const auto& mesh_component = entity.get_component<DynamicMesh>();
            if (!mesh_component.get_data())
                return false;

            return try_append_mesh_geometry(
                mesh_component.get_mesh(),
                Mat4(1.0F),
                scale,
                vertices,
                triangles);
        }

        if (!entity.has_component<StaticMesh>())
            return false;

        const auto& static_mesh = entity.get_component<StaticMesh>();
        if (!static_mesh.handle.is_valid())
            return false;

        auto model = asset_manager.load<Model>(static_mesh.handle);
        if (!model || model->meshes.empty())
            return false;

        if (model->parts.empty())
        {
            bool has_any_mesh = false;
            for (const auto& mesh : model->meshes)
                has_any_mesh |=
                    try_append_mesh_geometry(mesh, Mat4(1.0F), scale, vertices, triangles);

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

        auto queue = std::vector<MeshPartQueueEntry> {};
        queue.reserve(model->parts.size());
        for (size part_index = 0U; part_index < model->parts.size(); ++part_index)
        {
            if (has_parent[part_index])
                continue;

            queue.push_back(
                MeshPartQueueEntry {
                    .part_index = part_index,
                    .parent_transform = Mat4(1.0F),
                });
        }

        if (queue.empty())
        {
            queue.push_back(
                MeshPartQueueEntry {
                    .part_index = 0U,
                    .parent_transform = Mat4(1.0F),
                });
        }

        auto visited_parts = std::vector<bool>(model->parts.size(), false);
        bool has_any_part_mesh = false;
        while (!queue.empty())
        {
            const MeshPartQueueEntry current = queue.back();
            queue.pop_back();
            if (current.part_index >= model->parts.size())
                continue;

            if (visited_parts[current.part_index])
                continue;
            visited_parts[current.part_index] = true;

            const auto& part = model->parts[current.part_index];
            const Mat4 part_transform = current.parent_transform * part.transform;
            if (part.mesh_index < model->meshes.size())
            {
                has_any_part_mesh |= try_append_mesh_geometry(
                    model->meshes[part.mesh_index],
                    part_transform,
                    scale,
                    vertices,
                    triangles);
            }

            for (const auto child_index : part.children)
            {
                queue.push_back(
                    MeshPartQueueEntry {
                        .part_index = child_index,
                        .parent_transform = part_transform,
                    });
            }
        }

        return has_any_part_mesh;
    }

    static PhysicsColliderCreateInfo create_collider_info_for_entity(
        AssetManager& asset_manager,
        const Entity& entity,
        const Transform& transform,
        bool is_physics_driven)
    {
        auto create_info = PhysicsColliderCreateInfo {};
        create_info.is_trigger_only = is_trigger_only_collider(entity);

        if (entity.has_component<SphereCollider>())
        {
            const auto& sphere = entity.get_component<SphereCollider>();
            create_info.shape_type = PhysicsColliderShapeType::SPHERE;
            create_info.radius = sphere.radius;
            return create_info;
        }

        if (entity.has_component<CapsuleCollider>())
        {
            const auto& capsule = entity.get_component<CapsuleCollider>();
            create_info.shape_type = PhysicsColliderShapeType::CAPSULE;
            create_info.radius = capsule.radius;
            create_info.half_height = capsule.half_height;
            return create_info;
        }

        if (entity.has_component<CubeCollider>())
        {
            const auto& cube = entity.get_component<CubeCollider>();
            create_info.shape_type = PhysicsColliderShapeType::BOX;
            create_info.half_extents = cube.half_extents;
            return create_info;
        }

        if (entity.has_component<MeshCollider>())
        {
            const auto& mesh_collider = entity.get_component<MeshCollider>();
            create_info.shape_type = PhysicsColliderShapeType::MESH;
            create_info.is_convex = mesh_collider.is_convex || is_physics_driven;
            if (try_get_mesh_collider_data(
                    asset_manager,
                    entity,
                    transform.scale,
                    create_info.mesh_vertices,
                    create_info.mesh_triangles))
                return create_info;

            TBX_TRACE_WARNING(
                "Physics: tbx::MeshCollider on entity {} has no usable mesh geometry, using "
                "fallback box shape.",
                to_string(entity.get_id()));
        }

        create_info.shape_type = PhysicsColliderShapeType::BOX;
        create_info.half_extents = Vec3(0.5F, 0.5F, 0.5F);
        return create_info;
    }

}
