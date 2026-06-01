#include "tbx/systems/physics/physics.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/types/assets/model.h"
#include "tbx/types/components/collider.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/components/rigidbody.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/quaternions.h"

namespace tbx
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
        if (!static_mesh.handle.id.is_valid())
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
                entity.get_id());
        }

        create_info.shape_type = PhysicsColliderShapeType::BOX;
        create_info.half_extents = Vec3(0.5F, 0.5F, 0.5F);
        return create_info;
    }

    struct Physics::EntityRecord
    {
        PhysicsColliderHandle collider = {};
        PhysicsRigidbodyHandle rigidbody = {};
        Vec3 last_position = Vec3(0.0F, 0.0F, 0.0F);
        Quat last_rotation = Quat(1.0F, 0.0F, 0.0F, 0.0F);
        Vec3 last_scale = Vec3(1.0F, 1.0F, 1.0F);
        bool has_last_transform = false;
        bool is_physics_driven = false;
        bool is_trigger_only = false;
    };

    Physics::Physics(
        std::weak_ptr<IPhysicsBackend> backend,
        std::weak_ptr<AssetManager> asset_manager,
        std::weak_ptr<WorldManager> world_manager,
        const PhysicsSettings& settings)
        : _backend(std::move(backend))
        , _asset_manager(std::move(asset_manager))
        , _world_manager(std::move(world_manager))
    {
        if (auto backend_strong = _backend.lock())
            backend_strong->initialize(get_backend_settings(settings));
    }

    Physics::~Physics() noexcept
    {
        clear_resources();
        if (auto backend = _backend.lock())
            backend->shutdown();
    }

    RaycastResult Physics::raycast(const RaycastQuery& raycast_query) const
    {
        auto backend = _backend.lock();
        if (!backend)
            return {};

        auto ignored_rigidbody = PhysicsRigidbodyHandle {};
        if (raycast_query.ignore_entity && raycast_query.ignored_entity_id.is_valid())
        {
            if (const auto record_it =
                    _records_by_entity.find(raycast_query.ignored_entity_id);
                record_it != _records_by_entity.end())
                ignored_rigidbody = record_it->second.rigidbody;
        }

        auto backend_hit = PhysicsRaycastHit {};
        if (!backend->raycast(raycast_query, ignored_rigidbody, backend_hit) || !backend_hit)
            return {};

        return RaycastResult {
            .has_hit = true,
            .hit_entity_id = try_get_entity_for_rigidbody(backend_hit.rigidbody),
            .hit_position = backend_hit.hit_position,
            .hit_fraction = backend_hit.hit_fraction,
        };
    }

    void Physics::update(const DeltaTime& dt, const PhysicsSettings& settings)
    {
        if (_backend.expired())
            return;

        auto asset_manager = _asset_manager.lock();
        if (!asset_manager)
            return;

        auto worlds = std::vector<std::shared_ptr<World>> {};
        if (const auto world_manager = _world_manager.lock())
        {
            if (auto world = world_manager->get_active_world().lock())
                worlds.push_back(world);
        }
        else
        {
            worlds = asset_manager->get_loaded<World>();
        }

        for (const auto& world : worlds)
        {
            if (!world)
                continue;

            sync_entities_to_backend(*world, static_cast<float>(dt.seconds));
        }
        if (auto backend = _backend.lock())
            backend->update(get_backend_settings(settings), dt);
        for (const auto& world : worlds)
        {
            if (!world)
                continue;

            sync_backend_to_entities(*world);
            process_trigger_colliders(*world);
        }
    }

    void Physics::clear_resources()
    {
        for (auto& record_entry : _records_by_entity)
            destroy_record(record_entry.second);

        _records_by_entity.clear();
        _entity_by_rigidbody_handle.clear();
        _overlap_entities_by_trigger.clear();
    }

    void Physics::destroy_record(EntityRecord& record)
    {
        if (auto backend = _backend.lock())
        {
            if (record.rigidbody.is_valid())
                backend->destroy_rigidbody(record.rigidbody);

            if (record.collider.is_valid())
                backend->destroy_collider(record.collider);
        }

        _entity_by_rigidbody_handle.erase(record.rigidbody.value);
        record = {};
    }

    PhysicsBackendSettings Physics::get_backend_settings(const PhysicsSettings& settings)
    {
        return PhysicsBackendSettings {
            .gravity = settings.gravity,
            .max_body_count = settings.max_body_count,
            .max_contact_constraints = settings.max_contact_constraints,
            .max_body_pairs = settings.max_body_pairs,
            .solver_velocity_iterations = settings.solver_velocity_iterations,
            .solver_position_iterations = settings.solver_position_iterations,
            .max_linear_velocity = settings.max_linear_velocity,
            .max_angular_velocity = settings.max_angular_velocity,
        };
    }

    void Physics::process_trigger_colliders(World& world)
    {
        auto backend = _backend.lock();
        if (!backend)
            return;

        auto active_trigger_entities = std::unordered_set<Uuid>();
        auto trigger_entities = world.get_with<Transform>();
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
            if (const auto record_it = _records_by_entity.find(trigger_entity_id);
                record_it != _records_by_entity.end())
            {
                auto overlapped_rigidbodies = std::vector<PhysicsRigidbodyHandle> {};
                backend->get_rigidbody_overlaps(
                    record_it->second.rigidbody,
                    overlapped_rigidbodies);
                current_overlaps.reserve(overlapped_rigidbodies.size());
                for (const PhysicsRigidbodyHandle overlapped_rigidbody : overlapped_rigidbodies)
                {
                    const Uuid overlapped_entity_id =
                        try_get_entity_for_rigidbody(overlapped_rigidbody);
                    if (!overlapped_entity_id.is_valid()
                        || overlapped_entity_id == trigger_entity_id)
                        continue;

                    current_overlaps.insert(overlapped_entity_id);
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

    void Physics::sync_entities_to_backend(World& world, float dt_seconds)
    {
        auto asset_manager = _asset_manager.lock();
        auto backend = _backend.lock();
        if (!asset_manager || !backend)
            return;

        auto active_entities = std::unordered_set<Uuid>();

        auto entities = world.get_with<Transform>();
        for (auto& entity : entities)
        {
            const Uuid entity_id = entity.get_id();
            const auto world_transform = get_world_space_transform(entity);
            const bool has_rigidbody_component = entity.has_component<Rigidbody>();
            const bool has_collider = has_any_collider(entity);
            if (!has_rigidbody_component && !has_collider)
                continue;

            const bool is_trigger_only = is_trigger_only_collider(entity);
            const auto* rigidbody =
                has_rigidbody_component ? &entity.get_component<Rigidbody>() : nullptr;
            const bool is_physics_driven = rigidbody != nullptr && rigidbody->is_valid();
            if (has_rigidbody_component && !is_physics_driven)
                continue;

            active_entities.insert(entity_id);

            auto record_it = _records_by_entity.find(entity_id);
            if (record_it != _records_by_entity.end()
                && (record_it->second.is_physics_driven != is_physics_driven
                    || record_it->second.is_trigger_only != is_trigger_only
                    || (entity.has_component<MeshCollider>() && record_it->second.has_last_transform
                        && has_scale_changed(world_transform.scale, record_it->second.last_scale))))
            {
                destroy_record(record_it->second);
                _records_by_entity.erase(record_it);
                record_it = _records_by_entity.end();
            }

            if (record_it == _records_by_entity.end())
            {
                const PhysicsColliderCreateInfo collider_info = create_collider_info_for_entity(
                    *asset_manager,
                    entity,
                    world_transform,
                    is_physics_driven);
                PhysicsColliderHandle collider = backend->create_collider(collider_info);
                if (!collider.is_valid())
                    continue;

                const PhysicsRigidbodyCreateInfo rigidbody_info = PhysicsRigidbodyCreateInfo {
                    .collider = collider,
                    .transform = world_transform,
                    .rigidbody = rigidbody != nullptr ? *rigidbody : Rigidbody {},
                    .has_rigidbody = is_physics_driven,
                    .is_trigger_only = is_trigger_only,
                };
                PhysicsRigidbodyHandle rigidbody_handle = backend->create_rigidbody(rigidbody_info);
                if (!rigidbody_handle.is_valid())
                {
                    backend->destroy_collider(collider);
                    continue;
                }

                auto& record = _records_by_entity[entity_id];
                record.collider = collider;
                record.rigidbody = rigidbody_handle;
                record.last_position = world_transform.position;
                record.last_rotation = world_transform.rotation;
                record.last_scale = world_transform.scale;
                record.has_last_transform = true;
                record.is_physics_driven = is_physics_driven;
                record.is_trigger_only = is_trigger_only;
                _entity_by_rigidbody_handle[rigidbody_handle.value] = entity_id;
                continue;
            }

            auto& record = record_it->second;
            const bool transform_is_dirty = record.has_last_transform
                                            && has_transform_changed(
                                                world_transform,
                                                record.last_position,
                                                record.last_rotation,
                                                record.last_scale);
            auto update_info = PhysicsRigidbodyUpdateInfo {
                .transform = world_transform,
                .rigidbody = rigidbody != nullptr ? *rigidbody : Rigidbody {},
                .has_rigidbody = is_physics_driven,
                .is_trigger_only = is_trigger_only,
                .is_transform_dirty = transform_is_dirty,
                .dt_seconds = std::max(0.0001F, dt_seconds),
            };

            if (is_physics_driven && rigidbody != nullptr && !rigidbody->is_kinematic
                && transform_is_dirty
                && rigidbody->transform_sync_mode == PhysicsTransformSyncMode::SWEEP)
            {
                const PhysicsRigidbodyState current_state =
                    backend->get_rigidbody_state(record.rigidbody);
                const float safe_dt_seconds = std::max(0.0001F, dt_seconds);
                if (current_state.is_valid)
                {
                    update_info.sweep_linear_velocity =
                        (world_transform.position - current_state.transform.position)
                        / safe_dt_seconds;
                    update_info.sweep_angular_velocity = calculate_angular_velocity_for_step(
                        current_state.transform.rotation,
                        world_transform.rotation,
                        safe_dt_seconds);
                }
            }

            backend->update_rigidbody(record.rigidbody, update_info);
            if (!is_physics_driven || (rigidbody != nullptr && rigidbody->is_kinematic))
            {
                record.last_position = world_transform.position;
                record.last_rotation = world_transform.rotation;
                record.last_scale = world_transform.scale;
                record.has_last_transform = true;
            }
        }

        auto stale_entities = std::vector<Uuid>();
        stale_entities.reserve(_records_by_entity.size());
        for (const auto& record_entry : _records_by_entity)
        {
            if (active_entities.contains(record_entry.first))
                continue;

            stale_entities.push_back(record_entry.first);
        }

        for (const Uuid& stale_entity : stale_entities)
        {
            auto record_it = _records_by_entity.find(stale_entity);
            if (record_it == _records_by_entity.end())
                continue;

            destroy_record(record_it->second);
            _records_by_entity.erase(record_it);
        }
    }

    void Physics::sync_backend_to_entities(World& world)
    {
        auto backend = _backend.lock();
        if (!backend)
            return;

        for (auto& record_entry : _records_by_entity)
        {
            const Uuid& entity_id = record_entry.first;
            auto& record = record_entry.second;

            if (!world.has<Transform>(entity_id))
                continue;

            auto entity = world.get(entity_id);
            if (!entity.get_id().is_valid())
                continue;
            auto& transform = entity.get_component<Transform>();

            if (!world.has<Rigidbody>(entity_id))
            {
                const auto world_transform = get_world_space_transform(entity);
                record.last_position = world_transform.position;
                record.last_rotation = world_transform.rotation;
                record.last_scale = world_transform.scale;
                record.has_last_transform = true;
                continue;
            }

            PhysicsRigidbodyState state = backend->get_rigidbody_state(record.rigidbody);
            if (!state.is_valid)
                continue;

            auto& rigidbody = entity.get_component<Rigidbody>();
            rigidbody.linear_velocity = state.linear_velocity;
            rigidbody.angular_velocity = state.angular_velocity;

            if (rigidbody.is_kinematic)
            {
                const auto world_transform = get_world_space_transform(entity);
                record.last_position = world_transform.position;
                record.last_rotation = world_transform.rotation;
                record.last_scale = world_transform.scale;
                record.has_last_transform = true;
                continue;
            }

            auto world_transform = state.transform;
            world_transform.scale = transform.scale;
            auto parent_entity = Entity {};
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

            record.last_position = world_transform.position;
            record.last_rotation = world_transform.rotation;
            record.last_scale = world_transform.scale;
            record.has_last_transform = true;
        }
    }

    Uuid Physics::try_get_entity_for_rigidbody(PhysicsRigidbodyHandle rigidbody) const
    {
        auto entity_it = _entity_by_rigidbody_handle.find(rigidbody.value);
        if (entity_it == _entity_by_rigidbody_handle.end())
            return {};

        return entity_it->second;
    }
}
