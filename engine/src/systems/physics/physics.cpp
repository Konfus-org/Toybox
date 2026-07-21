#include "tbx/systems/physics/physics.h"
#include "tbx/interfaces/message_dispatcher.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/types/assets/model.h"
#include "tbx/types/components/collider.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/components/renderer.h"
#include "tbx/types/components/rigidbody.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/quaternions.h"
#include "tbx/types/vertex.h"
#include <algorithm>
#include <cmath>

namespace tbx
{
    // A solid collision shape of any kind.
    static bool has_collider_shape(const Entity& entity)
    {
        return entity.has_component<BoxCollider>() || entity.has_component<SphereCollider>()
               || entity.has_component<CapsuleCollider>() || entity.has_component<MeshCollider>();
    }

    // An overlap trigger of any shape.
    static bool has_trigger_shape(const Entity& entity)
    {
        return entity.has_component<BoxTrigger>() || entity.has_component<SphereTrigger>()
               || entity.has_component<CapsuleTrigger>() || entity.has_component<MeshTrigger>();
    }

    static bool has_any_collider(const Entity& entity)
    {
        return has_collider_shape(entity) || has_trigger_shape(entity);
    }

    // The shaped solid collider the entity carries, when any, as its shared Collider base — the
    // backend recovers the concrete shape from it.
    static const Collider* try_get_solid_collider(const Entity& entity)
    {
        if (entity.has_component<BoxCollider>())
            return &entity.get_component<BoxCollider>();
        if (entity.has_component<SphereCollider>())
            return &entity.get_component<SphereCollider>();
        if (entity.has_component<CapsuleCollider>())
            return &entity.get_component<CapsuleCollider>();
        if (entity.has_component<MeshCollider>())
            return &entity.get_component<MeshCollider>();
        return nullptr;
    }

    // The shared Trigger behavior of whichever shaped trigger the entity carries, when any.
    static Trigger* try_get_trigger_collider(Entity& entity)
    {
        if (entity.has_component<BoxTrigger>())
            return &entity.get_component<BoxTrigger>();
        if (entity.has_component<SphereTrigger>())
            return &entity.get_component<SphereTrigger>();
        if (entity.has_component<CapsuleTrigger>())
            return &entity.get_component<CapsuleTrigger>();
        if (entity.has_component<MeshTrigger>())
            return &entity.get_component<MeshTrigger>();
        return nullptr;
    }

    // Invokes the begin or end contact callbacks of every solid collider component the entity
    // carries (an entity can stack several shaped colliders on the shared Collider base).
    static void dispatch_contact_to_colliders(
        Entity& entity,
        const ColliderContactEvent& event,
        bool is_begin_phase)
    {
        const auto invoke_callbacks = [&event, is_begin_phase](Collider& collider)
        {
            const auto& callbacks =
                is_begin_phase ? collider.contact_begin_callbacks : collider.contact_end_callbacks;
            for (const auto& callback : callbacks)
            {
                if (callback)
                    callback(event);
            }
        };

        if (entity.has_component<BoxCollider>())
            invoke_callbacks(entity.get_component<BoxCollider>());
        if (entity.has_component<SphereCollider>())
            invoke_callbacks(entity.get_component<SphereCollider>());
        if (entity.has_component<CapsuleCollider>())
            invoke_callbacks(entity.get_component<CapsuleCollider>());
        if (entity.has_component<MeshCollider>())
            invoke_callbacks(entity.get_component<MeshCollider>());
    }

    // The backend body is a non-solid sensor when the entity has a trigger but no solid collider; an
    // entity with both gets a solid body.
    static bool is_trigger_only_collider(const Entity& entity)
    {
        return has_trigger_shape(entity) && !has_collider_shape(entity);
    }

    static bool uses_mesh_shape(const Entity& entity)
    {
        return entity.has_component<MeshCollider>() || entity.has_component<MeshTrigger>();
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

    // Config that shapes a backend body, excluding the velocities the read-back overwrites each
    // frame (comparing those would rebuild every dynamic body every step).
    static bool has_rigidbody_config_changed(const Rigidbody& current, const Rigidbody& previous)
    {
        return current.mass != previous.mass || current.is_kinematic != previous.is_kinematic
               || current.is_gravity_enabled != previous.is_gravity_enabled
               || current.friction != previous.friction
               || current.restitution != previous.restitution
               || current.linear_damping != previous.linear_damping
               || current.angular_damping != previous.angular_damping
               || current.is_sleep_enabled != previous.is_sleep_enabled
               || current.sleep_velocity_threshold != previous.sleep_velocity_threshold
               || current.sleep_time_seconds != previous.sleep_time_seconds;
    }

    // A rigidbody value whose is_valid() is false, so create_rigidbody builds a static body for an
    // entity that has a collider/trigger but no live Rigidbody component.
    static Rigidbody make_static_body_marker()
    {
        auto marker = Rigidbody {};
        marker.mass = 0.0F;
        return marker;
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
            if (attribute.type != VertexFormat::VEC3)
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
        std::vector<uint32>& indices)
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
            indices.reserve(indices.size() + triangle_count * 3U);
            for (size triangle_index = 0U; triangle_index < triangle_count; ++triangle_index)
            {
                const size index_base = triangle_index * 3U;
                const size index0 = base_vertex_index + mesh_indices[index_base];
                const size index1 = base_vertex_index + mesh_indices[index_base + 1U];
                const size index2 = base_vertex_index + mesh_indices[index_base + 2U];
                if (index0 >= vertices.size() || index1 >= vertices.size()
                    || index2 >= vertices.size())
                    continue;

                indices.push_back(static_cast<uint32>(index0));
                indices.push_back(static_cast<uint32>(index1));
                indices.push_back(static_cast<uint32>(index2));
            }
        }

        return vertices.size() > base_vertex_index;
    }

    struct MeshPartQueueEntry
    {
        size part_index = 0U;
        Mat4 parent_transform = Mat4(1.0F);
    };

    static bool try_gather_model_geometry(
        AssetManager& asset_manager,
        const Entity& entity,
        const Vec3& scale,
        std::vector<Vec3>& vertices,
        std::vector<uint32>& indices)
    {
        vertices.clear();
        indices.clear();

        if (!entity.has_component<Renderer>())
            return false;

        const auto& renderer = entity.get_component<Renderer>();
        if (!renderer.model.handle.id.is_valid())
            return false;

        auto model = asset_manager.load<Model>(renderer.model);
        if (!model || model->meshes.empty())
            return false;

        if (model->parts.empty())
        {
            bool has_any_mesh = false;
            for (const auto& mesh : model->meshes)
                has_any_mesh |= try_append_mesh_geometry(mesh, Mat4(1.0F), scale, vertices, indices);

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
                    indices);
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

    // Builds the flattened, scale-baked collision mesh a mesh collider/trigger hands to the backend.
    static bool try_build_collision_mesh(
        AssetManager& asset_manager,
        const Entity& entity,
        const Vec3& scale,
        Mesh& out_mesh)
    {
        auto vertices = std::vector<Vec3> {};
        auto indices = std::vector<uint32> {};
        if (!try_gather_model_geometry(asset_manager, entity, scale, vertices, indices))
            return false;

        auto floats = std::vector<float> {};
        floats.reserve(vertices.size() * 3U);
        for (const auto& vertex : vertices)
        {
            floats.push_back(vertex.x);
            floats.push_back(vertex.y);
            floats.push_back(vertex.z);
        }

        out_mesh = Mesh {};
        out_mesh.vertices.layout =
            VertexBufferLayout(std::vector<VertexData> {VertexFormat::VEC3});
        out_mesh.vertices.vertices = std::move(floats);
        out_mesh.indices = std::move(indices);
        return !out_mesh.vertices.vertices.empty();
    }

    static Entity find_entity_in_worlds(
        const std::vector<std::shared_ptr<World>>& worlds, const Uuid& entity_id)
    {
        for (const auto& world : worlds)
        {
            if (world && world->has(entity_id))
                return world->get(entity_id);
        }
        return Entity {};
    }

    struct Physics::EntityRecord
    {
        PhysicsHandle shape_handle = {};
        PhysicsHandle body_handle = {};
        Vec3 last_position = Vec3(0.0F, 0.0F, 0.0F);
        Quat last_rotation = Quat(1.0F, 0.0F, 0.0F, 0.0F);
        Vec3 last_scale = Vec3(1.0F, 1.0F, 1.0F);
        bool has_last_transform = false;
        bool is_physics_driven = false;
        bool is_trigger_only = false;
        Rigidbody last_rigidbody = {};
    };

    void Physics::EntityRecordDeleter::operator()(EntityRecord* record) const noexcept
    {
        delete record;
    }

    constexpr auto PHYSICS_LANE_NAME = std::string_view("physics");

    Physics::Physics(
        std::weak_ptr<IPhysicsBackend> backend,
        std::weak_ptr<AssetManager> asset_manager,
        std::weak_ptr<WorldManager> world_manager,
        std::weak_ptr<ThreadManager> thread_manager,
        const PhysicsSettings& settings)
        : Physics(
              std::move(backend),
              std::move(asset_manager),
              std::move(world_manager),
              std::move(thread_manager),
              std::weak_ptr<IMessageCoordinator>(),
              settings)
    {
    }

    Physics::Physics(
        std::weak_ptr<IPhysicsBackend> backend,
        std::weak_ptr<AssetManager> asset_manager,
        std::weak_ptr<WorldManager> world_manager,
        std::weak_ptr<ThreadManager> thread_manager,
        std::weak_ptr<IMessageCoordinator> message_coordinator,
        const PhysicsSettings& settings)
        : _backend(std::move(backend))
        , _asset_manager(std::move(asset_manager))
        , _message_coordinator(message_coordinator)
        , _world_manager(std::move(world_manager))
        , _thread_manager(std::move(thread_manager))
    {
        if (auto coordinator = _message_coordinator.lock())
        {
            _asset_reload_handler = coordinator->register_handler(
                [this](Message& message)
                {
                    if (const auto reloaded = handle_message<AssetReloadedEvent>(message))
                        on_asset_reloaded(reloaded->get());
                });
        }

        // The simulation step runs on its own lane so it overlaps frame work on the main thread.
        // Without the lane (e.g. headless tooling that omits the thread manager) the step still runs
        // synchronously inside dispatch_step, so physics stays correct either way.
        if (auto thread_manager_service = _thread_manager.lock())
        {
            _has_physics_lane = thread_manager_service->has_lane(PHYSICS_LANE_NAME)
                                || thread_manager_service->try_create_lane(PHYSICS_LANE_NAME);
            if (!_has_physics_lane)
                TBX_TRACE_WARNING(
                    "Physics: failed to create the physics lane; simulation will run on the main "
                    "thread.");
        }

        if (auto backend_strong = _backend.lock())
        {
            backend_strong->initialize(
                settings.gravity,
                settings.max_body_count,
                settings.max_contact_constraints,
                settings.max_body_pairs,
                settings.solver_velocity_iterations,
                settings.solver_position_iterations,
                settings.max_linear_velocity,
                settings.max_angular_velocity);
        }
    }

    Physics::~Physics() noexcept
    {
        // The backend is about to be torn down, so the in-flight step must finish touching it first.
        wait_for_pending_step();

        if (auto coordinator = _message_coordinator.lock())
            coordinator->deregister_handler(_asset_reload_handler);

        if (_has_physics_lane)
        {
            if (auto thread_manager = _thread_manager.lock())
                thread_manager->stop_lane(PHYSICS_LANE_NAME);
        }

        clear_resources();
        if (auto backend = _backend.lock())
            backend->shutdown();
    }

    RaycastResult Physics::raycast(const RaycastQuery& raycast_query) const
    {
        auto backend = _backend.lock();
        if (!backend)
            return {};

        // Backend queries cannot run while a simulation step is in flight, so join it first.
        wait_for_pending_step();

        auto ignored = std::vector<PhysicsHandle> {};
        if (raycast_query.ignore_entity && raycast_query.ignored_entity_id.is_valid())
        {
            if (const auto record_it = _records_by_entity.find(raycast_query.ignored_entity_id);
                record_it != _records_by_entity.end())
                ignored.push_back(record_it->second->body_handle);
        }

        auto backend_hit = PhysicsRaycastHit {};
        if (!backend->raycast(raycast_query, ignored, backend_hit) || !backend_hit)
            return {};

        return RaycastResult {
            .has_hit = true,
            .hit_entity_id = try_get_entity_for_body(backend_hit.rigidbody),
            .hit_position = backend_hit.hit_position,
        };
    }

    std::vector<Vec3> Physics::get_shape(const Uuid& entity_id) const
    {
        auto backend = _backend.lock();
        if (!backend)
            return {};

        // Backend queries cannot run while a simulation step is in flight, so join it first.
        wait_for_pending_step();

        const auto record_it = _records_by_entity.find(entity_id);
        if (record_it == _records_by_entity.end() || !record_it->second->shape_handle.is_valid())
            return {};

        return backend->get_debug_shape(record_it->second->shape_handle);
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

        // The simulation step for the previous call ran on the physics lane while the rest of the
        // frame (rendering, asset work) proceeded. Join it now and commit its results to the ECS
        // before reading the latest entity state back into the backend.
        wait_for_pending_step();
        if (_results_pending)
        {
            process_state(worlds);
            _results_pending = false;
        }

        for (const auto& world : worlds)
        {
            if (!world)
                continue;

            sync_entities_to_backend(*world);
        }
        _pending_model_reloads.clear();

        // The backend is idle here (step joined), so settings can be pushed safely before the next
        // step is dispatched.
        apply_backend_settings(settings);
        dispatch_step(dt);
    }

    void Physics::reset()
    {
        if (_backend.expired())
            return;

        // Join and discard the in-flight step: its result is from the world as it was before the
        // reset, so committing it (the _results_pending path in update) would write a stale
        // simulation step over the freshly-replaced entity state.
        wait_for_pending_step();
        _results_pending = false;

        // Destroy every backend body/shape and forget all tracking; the next sync rebuilds them
        // from the live entity transforms, so no position or velocity survives the reset.
        clear_resources();
        _pending_model_reloads.clear();
    }

    void Physics::apply_backend_settings(const PhysicsSettings& settings) const
    {
        auto backend = _backend.lock();
        if (!backend)
            return;

        backend->set_gravity(settings.gravity);
        backend->set_solver_velocity_iterations(settings.solver_velocity_iterations);
        backend->set_solver_position_iterations(settings.solver_position_iterations);
        backend->set_max_linear_velocity(settings.max_linear_velocity);
        backend->set_max_angular_velocity(settings.max_angular_velocity);
    }

    void Physics::dispatch_step(const DeltaTime& dt)
    {
        auto backend = _backend.lock();
        if (!backend)
            return;

        if (_has_physics_lane)
        {
            if (auto thread_manager = _thread_manager.lock())
            {
                _pending_step = thread_manager->post_with_future(
                    PHYSICS_LANE_NAME,
                    [backend, dt]() { backend->step(dt); });
                _results_pending = true;
                return;
            }
        }

        // No lane available: run synchronously so physics still advances.
        backend->step(dt);
        _results_pending = true;
    }

    void Physics::wait_for_pending_step() const noexcept
    {
        if (!_pending_step.valid())
            return;

        TBX_TRY_CATCH_ASSERT(_pending_step.get();, "Physics simulation step failed.");
    }

    void Physics::clear_resources()
    {
        for (auto& record_entry : _records_by_entity)
            destroy_record(*record_entry.second);

        _records_by_entity.clear();
        _entity_by_body_handle.clear();
        _overlap_entities_by_trigger.clear();
        _contacts_by_entity.clear();
    }

    void Physics::destroy_record(EntityRecord& record)
    {
        if (auto backend = _backend.lock())
        {
            if (record.body_handle.is_valid())
                backend->destroy(record.body_handle);

            if (record.shape_handle.is_valid())
                backend->destroy(record.shape_handle);
        }

        _entity_by_body_handle.erase(record.body_handle.id);
        record = {};
    }

    void Physics::process_state(const std::vector<std::shared_ptr<World>>& worlds)
    {
        auto backend = _backend.lock();
        if (!backend)
            return;

        for (auto& record_entry : _records_by_entity)
        {
            const Uuid& entity_id = record_entry.first;
            auto& record = *record_entry.second;
            if (!record.body_handle.is_valid())
                continue;

            auto state = PhysicsEntityState {};
            if (!backend->get_state(record.body_handle, state))
                continue;

            auto entity = find_entity_in_worlds(worlds, entity_id);
            if (!entity.get_id().is_valid() || !entity.has_component<Transform>())
                continue;

            // --- read the simulated pose/velocity back into the entity ---
            if (entity.has_component<Rigidbody>())
            {
                auto& rigidbody = entity.get_component<Rigidbody>();
                rigidbody.linear_velocity = state.linear_velocity;
                rigidbody.angular_velocity = state.angular_velocity;

                // Only a dynamic body is moved by the simulation; a kinematic body's pose is owned
                // by scripts (sync destroys+recreates it when they move it), so it is left alone.
                if (!rigidbody.is_kinematic)
                {
                    auto& transform = entity.get_component<Transform>();
                    auto world_transform = state.transform;
                    world_transform.scale = transform.scale;

                    auto parent_entity = Entity {};
                    if (entity.try_get_parent_entity(parent_entity)
                        && parent_entity.has_component<Transform>())
                    {
                        const auto parent_world_transform =
                            parent_entity.get_component<Transform>().to_world_space(parent_entity);
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

            // --- turn the current contact/overlap sets into begin/stay/end callbacks ---
            if (has_collider_shape(entity))
            {
                auto current_contacts = std::unordered_set<Uuid> {};
                auto& previous_contacts = _contacts_by_entity[entity_id];
                for (const auto& contact : state.contacts)
                {
                    const Uuid other_entity_id = try_get_entity_for_body(contact.other);
                    if (!other_entity_id.is_valid() || other_entity_id == entity_id)
                        continue;

                    current_contacts.insert(other_entity_id);
                    if (!previous_contacts.contains(other_entity_id))
                    {
                        dispatch_contact_to_colliders(
                            entity,
                            ColliderContactEvent {
                                .entity_id = entity_id,
                                .other_entity_id = other_entity_id,
                                .position = contact.position,
                                .normal = contact.normal,
                            },
                            true);
                    }
                }

                for (const Uuid& other_entity_id : previous_contacts)
                {
                    if (current_contacts.contains(other_entity_id))
                        continue;

                    dispatch_contact_to_colliders(
                        entity,
                        ColliderContactEvent {
                            .entity_id = entity_id,
                            .other_entity_id = other_entity_id,
                        },
                        false);
                }

                if (current_contacts.empty())
                    _contacts_by_entity.erase(entity_id);
                else
                    previous_contacts = std::move(current_contacts);
            }

            if (auto* trigger = try_get_trigger_collider(entity))
            {
                if (!trigger->is_overlap_enabled)
                {
                    trigger->is_manual_scan_requested = false;
                    trigger->occupant_count = 0;
                    _overlap_entities_by_trigger.erase(entity_id);
                }
                else if (should_execute_overlap_query(
                             trigger->overlap_execution_mode, trigger->is_manual_scan_requested))
                {
                    trigger->is_manual_scan_requested = false;

                    auto current_overlaps = std::unordered_set<Uuid> {};
                    for (const auto& overlapped_handle : state.overlaps)
                    {
                        const Uuid overlapped_entity_id =
                            try_get_entity_for_body(overlapped_handle);
                        if (!overlapped_entity_id.is_valid() || overlapped_entity_id == entity_id)
                            continue;

                        current_overlaps.insert(overlapped_entity_id);
                    }

                    auto& previous_overlaps = _overlap_entities_by_trigger[entity_id];
                    for (const Uuid& overlapped_entity_id : current_overlaps)
                    {
                        const auto event = ColliderOverlapEvent {
                            .trigger_entity_id = entity_id,
                            .overlapped_entity_id = overlapped_entity_id,
                        };
                        const bool was_overlapping = previous_overlaps.contains(overlapped_entity_id);
                        const auto& callbacks = was_overlapping ? trigger->overlap_stay_callbacks
                                                                : trigger->overlap_begin_callbacks;
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

                        const auto event = ColliderOverlapEvent {
                            .trigger_entity_id = entity_id,
                            .overlapped_entity_id = overlapped_entity_id,
                        };
                        for (const auto& callback : trigger->overlap_end_callbacks)
                        {
                            if (callback)
                                callback(event);
                        }
                    }

                    trigger->occupant_count = static_cast<size>(current_overlaps.size());
                    if (current_overlaps.empty())
                        _overlap_entities_by_trigger.erase(entity_id);
                    else
                        previous_overlaps = std::move(current_overlaps);
                }
                else
                {
                    trigger->is_manual_scan_requested = false;
                }
            }
        }
    }

    void Physics::sync_entities_to_backend(World& world)
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
            const bool has_rigidbody_component = entity.has_component<Rigidbody>();
            const bool has_collider = has_any_collider(entity);
            if (!has_rigidbody_component && !has_collider)
                continue;

            const auto* rigidbody =
                has_rigidbody_component ? &entity.get_component<Rigidbody>() : nullptr;
            const bool is_physics_driven = rigidbody != nullptr && rigidbody->is_valid();
            if (has_rigidbody_component && !is_physics_driven)
                continue;

            const bool is_trigger_only = is_trigger_only_collider(entity);
            const auto world_transform = entity.get_component<Transform>().to_world_space(entity);
            active_entities.insert(entity_id);

            auto record_it = _records_by_entity.find(entity_id);
            if (record_it != _records_by_entity.end())
            {
                auto& record = *record_it->second;
                const bool needs_rebuild =
                    record.is_physics_driven != is_physics_driven
                    || record.is_trigger_only != is_trigger_only
                    || (entity.has_component<Renderer>()
                        && _pending_model_reloads.contains(
                            entity.get_component<Renderer>().model.handle.id))
                    || (uses_mesh_shape(entity) && record.has_last_transform
                        && has_scale_changed(world_transform.scale, record.last_scale))
                    || (record.has_last_transform
                        && has_transform_changed(
                            world_transform,
                            record.last_position,
                            record.last_rotation,
                            record.last_scale))
                    || (is_physics_driven
                        && has_rigidbody_config_changed(*rigidbody, record.last_rigidbody));
                if (!needs_rebuild)
                    continue;

                destroy_record(record);
                _records_by_entity.erase(record_it);
            }

            // --- (re)create the body: define the shape, then place it ---
            auto shape_handle = PhysicsHandle {};
            if (has_collider_shape(entity))
            {
                const Collider* collider = try_get_solid_collider(entity);
                auto collision_mesh = Mesh {};
                const Mesh* mesh = &Mesh::EMPTY;
                if (entity.has_component<MeshCollider>()
                    && try_build_collision_mesh(
                        *asset_manager, entity, world_transform.scale, collision_mesh))
                    mesh = &collision_mesh;
                if (collider != nullptr)
                    shape_handle = backend->create_collider(*collider, *mesh);
            }
            else if (has_trigger_shape(entity))
            {
                auto* trigger = try_get_trigger_collider(entity);
                auto collision_mesh = Mesh {};
                const Mesh* mesh = &Mesh::EMPTY;
                if (entity.has_component<MeshTrigger>()
                    && try_build_collision_mesh(
                        *asset_manager, entity, world_transform.scale, collision_mesh))
                    mesh = &collision_mesh;
                if (trigger != nullptr)
                    shape_handle = backend->create_trigger(*trigger, *mesh);
            }

            const Rigidbody body_rigidbody =
                is_physics_driven ? *rigidbody : make_static_body_marker();
            const PhysicsHandle body_handle =
                backend->create_rigidbody(world_transform, body_rigidbody);
            if (!body_handle.is_valid())
            {
                if (shape_handle.is_valid())
                    backend->destroy(shape_handle);
                continue;
            }

            auto record_ptr = EntityRecordPtr(new EntityRecord());
            auto& record = *record_ptr;
            record.shape_handle = shape_handle;
            record.body_handle = body_handle;
            record.last_position = world_transform.position;
            record.last_rotation = world_transform.rotation;
            record.last_scale = world_transform.scale;
            record.has_last_transform = true;
            record.is_physics_driven = is_physics_driven;
            record.is_trigger_only = is_trigger_only;
            record.last_rigidbody = body_rigidbody;
            _records_by_entity[entity_id] = std::move(record_ptr);
            _entity_by_body_handle[body_handle.id] = entity_id;
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

            destroy_record(*record_it->second);
            _records_by_entity.erase(record_it);
            _contacts_by_entity.erase(stale_entity);
            _overlap_entities_by_trigger.erase(stale_entity);
        }
    }

    Uuid Physics::try_get_entity_for_body(const PhysicsHandle& body_handle) const
    {
        const auto entity_it = _entity_by_body_handle.find(body_handle.id);
        if (entity_it == _entity_by_body_handle.end())
            return {};

        return entity_it->second;
    }

    void Physics::on_asset_reloaded(const AssetReloadedEvent& event)
    {
        if (!event.succeeded || !event.affected_asset.id.is_valid())
            return;

        _pending_model_reloads.insert(event.affected_asset.id);
    }
}
