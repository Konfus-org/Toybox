#include "jolt_physics_backend.h"
#include "jolt_collision_layers.h"
#include "jolt_runtime_lifetime.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/types/components/collider.h"
#include "tbx/types/vertex.h"
#include <algorithm>
#include <cstdint>

// clang-format off
#include <Jolt/Geometry/AABox.h>
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
// clang-format on

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

    static JPH::RefConst<JPH::Shape> make_box_shape(const tbx::Vec3& half_extents)
    {
        return new JPH::BoxShape(
            JPH::Vec3(
                std::max(0.001F, half_extents.x),
                std::max(0.001F, half_extents.y),
                std::max(0.001F, half_extents.z)));
    }

    static JPH::RefConst<JPH::Shape> make_sphere_shape(float radius)
    {
        return new JPH::SphereShape(std::max(0.001F, radius));
    }

    static JPH::RefConst<JPH::Shape> make_capsule_shape(float radius, float half_height)
    {
        return new JPH::CapsuleShape(std::max(0.001F, half_height), std::max(0.001F, radius));
    }

    // Reads shape-local vertex positions (and triangles) from a collision mesh built by the engine.
    // The engine has already baked any entity scale and model part transforms into these points.
    static bool extract_mesh_geometry(
        const tbx::Mesh& mesh,
        std::vector<JPH::Vec3>& out_points,
        std::vector<JPH::IndexedTriangle>& out_triangles)
    {
        const auto& buffer = mesh.vertices;
        const auto& values = buffer.vertices;
        const uint32 stride =
            buffer.layout.stride / static_cast<uint32>(sizeof(float));
        if (stride == 0U || values.empty() || (values.size() % stride) != 0U)
            return false;

        const uint32 position_offset =
            tbx::try_get_vertex_attribute_offset(
                buffer.layout, tbx::vertex_attribute_position_debug_name)
                .value_or(0U);
        if (position_offset + 2U >= stride)
            return false;

        const auto vertex_count = static_cast<uint32>(values.size() / stride);
        out_points.reserve(vertex_count);
        for (uint32 vertex = 0U; vertex < vertex_count; ++vertex)
        {
            const size base =
                static_cast<size>(vertex) * stride + position_offset;
            out_points.push_back(
                JPH::Vec3(values[base], values[base + 1U], values[base + 2U]));
        }

        for (size index = 0U; index + 2U < mesh.indices.size(); index += 3U)
        {
            const uint32 index0 = mesh.indices[index];
            const uint32 index1 = mesh.indices[index + 1U];
            const uint32 index2 = mesh.indices[index + 2U];
            if (index0 >= vertex_count || index1 >= vertex_count || index2 >= vertex_count)
                continue;

            out_triangles.push_back(JPH::IndexedTriangle(index0, index1, index2, 0U));
        }

        return !out_points.empty();
    }

    static JPH::RefConst<JPH::Shape> make_mesh_shape(const tbx::Mesh& mesh, bool is_convex)
    {
        std::vector<JPH::Vec3> points = {};
        std::vector<JPH::IndexedTriangle> triangles = {};
        if (!extract_mesh_geometry(mesh, points, triangles))
            return make_box_shape(tbx::Vec3(0.5F, 0.5F, 0.5F));

        // Convex hulls (also the only option for a body that moves) collide against anything; a
        // triangle mesh is concave but static-only, so it is used only when the collider asks for it.
        if (is_convex || triangles.empty())
        {
            JPH::Array<JPH::Vec3> hull_points = {};
            hull_points.reserve(static_cast<JPH::uint>(points.size()));
            for (const auto& point : points)
                hull_points.push_back(point);

            auto result = JPH::ConvexHullShapeSettings(hull_points).Create();
            if (result.HasError())
            {
                TBX_TRACE_WARNING(
                    "Jolt physics: failed to build convex mesh collider: {}",
                    result.GetError().c_str());
                return make_box_shape(tbx::Vec3(0.5F, 0.5F, 0.5F));
            }
            return result.Get();
        }

        JPH::VertexList vertex_list = {};
        vertex_list.reserve(static_cast<JPH::uint>(points.size()));
        for (const auto& point : points)
            vertex_list.push_back(JPH::Float3(point.GetX(), point.GetY(), point.GetZ()));

        JPH::IndexedTriangleList triangle_list = {};
        triangle_list.reserve(static_cast<JPH::uint>(triangles.size()));
        for (const auto& triangle : triangles)
            triangle_list.push_back(triangle);

        auto result =
            JPH::MeshShapeSettings(std::move(vertex_list), std::move(triangle_list)).Create();
        if (result.HasError())
        {
            TBX_TRACE_WARNING(
                "Jolt physics: failed to build mesh collider: {}",
                result.GetError().c_str());
            return make_box_shape(tbx::Vec3(0.5F, 0.5F, 0.5F));
        }
        return result.Get();
    }

    // The concrete shape is recovered from the polymorphic collider/trigger type; the mesh supplies
    // geometry for mesh shapes only.
    static JPH::RefConst<JPH::Shape> build_collider_shape(
        const tbx::Collider& collider, const tbx::Mesh& mesh)
    {
        if (const auto* box = dynamic_cast<const tbx::BoxCollider*>(&collider))
            return make_box_shape(box->half_extents);
        if (const auto* sphere = dynamic_cast<const tbx::SphereCollider*>(&collider))
            return make_sphere_shape(sphere->radius);
        if (const auto* capsule = dynamic_cast<const tbx::CapsuleCollider*>(&collider))
            return make_capsule_shape(capsule->radius, capsule->half_height);
        if (const auto* mesh_collider = dynamic_cast<const tbx::MeshCollider*>(&collider))
            return make_mesh_shape(mesh, mesh_collider->is_convex);

        return make_box_shape(tbx::Vec3(0.5F, 0.5F, 0.5F));
    }

    static JPH::RefConst<JPH::Shape> build_trigger_shape(
        const tbx::Trigger& trigger, const tbx::Mesh& mesh)
    {
        if (const auto* box = dynamic_cast<const tbx::BoxTrigger*>(&trigger))
            return make_box_shape(box->half_extents);
        if (const auto* sphere = dynamic_cast<const tbx::SphereTrigger*>(&trigger))
            return make_sphere_shape(sphere->radius);
        if (const auto* capsule = dynamic_cast<const tbx::CapsuleTrigger*>(&trigger))
            return make_capsule_shape(capsule->radius, capsule->half_height);
        if (const auto* mesh_trigger = dynamic_cast<const tbx::MeshTrigger*>(&trigger))
            return make_mesh_shape(mesh, mesh_trigger->is_convex);

        return make_box_shape(tbx::Vec3(0.5F, 0.5F, 0.5F));
    }

    static JPH::EMotionType get_motion_type(const tbx::Rigidbody& rigidbody, bool physics_driven)
    {
        if (!physics_driven)
            return JPH::EMotionType::Static;
        if (rigidbody.is_kinematic)
            return JPH::EMotionType::Kinematic;
        return JPH::EMotionType::Dynamic;
    }

    static void apply_dynamic_body_settings(
        const tbx::Rigidbody& rigidbody,
        bool is_sensor,
        float max_linear_velocity,
        float max_angular_velocity,
        JPH::BodyCreationSettings& out_body_settings)
    {
        out_body_settings.mIsSensor = is_sensor;
        out_body_settings.mAllowSleeping = rigidbody.is_sleep_enabled;
        out_body_settings.mFriction = rigidbody.friction;
        out_body_settings.mRestitution = rigidbody.restitution;
        out_body_settings.mLinearDamping = rigidbody.linear_damping;
        out_body_settings.mAngularDamping = rigidbody.angular_damping;
        out_body_settings.mLinearVelocity = to_jolt_vec3(rigidbody.linear_velocity);
        out_body_settings.mAngularVelocity = to_jolt_vec3(rigidbody.angular_velocity);
        out_body_settings.mGravityFactor = rigidbody.is_gravity_enabled ? 1.0F : 0.0F;
        out_body_settings.mMaxLinearVelocity = std::max(0.0F, max_linear_velocity);
        out_body_settings.mMaxAngularVelocity = std::max(0.0F, max_angular_velocity);
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

    void JoltPhysicsBackend::initialize(
        tbx::Vec3 gravity,
        uint32 max_body_count,
        uint32 max_contact_constraints,
        uint32 max_body_pairs,
        uint32 solver_velocity_iterations,
        uint32 solver_position_iterations,
        float max_linear_velocity,
        float max_angular_velocity)
    {
        if (_is_ready)
            return;

        if (!JoltRuntimeLifetime::acquire())
        {
            TBX_TRACE_ERROR("Jolt physics: failed to initialize Jolt runtime.");
            return;
        }

        _gravity = gravity;
        _solver_velocity_iterations = solver_velocity_iterations;
        _solver_position_iterations = solver_position_iterations;
        _max_linear_velocity = max_linear_velocity;
        _max_angular_velocity = max_angular_velocity;

        constexpr JPH::uint temp_allocator_bytes = 64U * 1024U * 1024U;
        _temp_allocator =
            std::make_unique<JPH::TempAllocatorImplWithMallocFallback>(temp_allocator_bytes);
        _job_system = std::make_unique<JPH::JobSystemThreadPool>(
            JPH::cMaxPhysicsJobs,
            JPH::cMaxPhysicsBarriers);

        _physics_system.Init(
            std::max<std::uint32_t>(1U, max_body_count),
            0,
            std::max<std::uint32_t>(1U, max_body_pairs),
            std::max<std::uint32_t>(1U, max_contact_constraints),
            get_broad_phase_layer_interface(),
            get_object_vs_broad_phase_layer_filter(),
            get_object_layer_pair_filter());
        _physics_system.SetContactListener(&_contact_listener);
        _physics_system.SetGravity(to_jolt_vec3(_gravity));
        apply_solver_settings();
        _is_ready = true;
    }

    void JoltPhysicsBackend::shutdown()
    {
        if (!_is_ready && !_temp_allocator && !_job_system)
            return;

        _physics_system.SetContactListener(nullptr);
        clear_resources();
        _job_system.reset();
        _temp_allocator.reset();
        _is_ready = false;
        JoltRuntimeLifetime::release();
    }

    void JoltPhysicsBackend::set_gravity(tbx::Vec3 gravity)
    {
        _gravity = gravity;
        if (_is_ready)
            _physics_system.SetGravity(to_jolt_vec3(_gravity));
    }

    void JoltPhysicsBackend::set_solver_velocity_iterations(uint32 iterations)
    {
        _solver_velocity_iterations = iterations;
        if (_is_ready)
            apply_solver_settings();
    }

    void JoltPhysicsBackend::set_solver_position_iterations(uint32 iterations)
    {
        _solver_position_iterations = iterations;
        if (_is_ready)
            apply_solver_settings();
    }

    void JoltPhysicsBackend::set_max_linear_velocity(float max_linear_velocity)
    {
        // Applies to bodies created after this point; existing bodies keep their creation limit.
        _max_linear_velocity = max_linear_velocity;
    }

    void JoltPhysicsBackend::set_max_angular_velocity(float max_angular_velocity)
    {
        _max_angular_velocity = max_angular_velocity;
    }

    void JoltPhysicsBackend::apply_solver_settings()
    {
        auto jolt_settings = _physics_system.GetPhysicsSettings();
        jolt_settings.mNumVelocitySteps =
            std::max<std::uint32_t>(2U, _solver_velocity_iterations);
        jolt_settings.mNumPositionSteps =
            std::max<std::uint32_t>(1U, _solver_position_iterations);
        _physics_system.SetPhysicsSettings(jolt_settings);
    }

    void JoltPhysicsBackend::step(const tbx::DeltaTime& dt)
    {
        if (!_is_ready || !_temp_allocator || !_job_system)
            return;

        const JPH::EPhysicsUpdateError update_error = _physics_system.Update(
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

    bool JoltPhysicsBackend::get_state(
        const tbx::PhysicsHandle& handle, tbx::PhysicsEntityState& out_state)
    {
        out_state = {};
        const auto body_it = _bodies.find(handle.id);
        if (body_it == _bodies.end())
            return false;

        const auto& body_interface = _physics_system.GetBodyInterface();
        const JPH::BodyID body_id = body_it->second.body_id;
        if (!body_interface.IsAdded(body_id))
            return false;

        out_state.transform.position = to_tbx_vec3_from_rvec3(body_interface.GetPosition(body_id));
        out_state.transform.rotation = to_tbx_quat(body_interface.GetRotation(body_id));
        // Jolt carries no scale; the engine keeps the entity's own scale, so report unit here.
        out_state.transform.scale = tbx::Vec3(1.0F, 1.0F, 1.0F);
        out_state.linear_velocity = to_tbx_vec3(body_interface.GetLinearVelocity(body_id));
        out_state.angular_velocity = to_tbx_vec3(body_interface.GetAngularVelocity(body_id));

        auto neighbors = std::vector<JoltContactNeighbor>();
        _contact_listener.get_contacts(get_body_key(body_id), neighbors);
        out_state.contacts.reserve(neighbors.size());
        for (const auto& neighbor : neighbors)
        {
            const tbx::PhysicsHandle other = try_get_body_handle(neighbor.other_body_key);
            if (!other.is_valid())
                continue;

            out_state.contacts.push_back(
                tbx::PhysicsContact {
                    .other = other,
                    .position = to_tbx_vec3_from_rvec3(neighbor.position),
                    .normal = to_tbx_vec3(neighbor.normal),
                });
        }

        if (body_it->second.is_sensor)
            gather_overlaps(body_it->second, out_state.overlaps);

        return true;
    }

    void JoltPhysicsBackend::gather_overlaps(
        const JoltBodyResource& body, std::vector<tbx::PhysicsHandle>& out_overlaps) const
    {
        const auto& body_interface = _physics_system.GetBodyInterface();
        const auto& narrow_phase_query = _physics_system.GetNarrowPhaseQuery();
        const JPH::BodyID body_id = body.body_id;
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
        for (const auto& hit : collector.mHits)
        {
            const tbx::PhysicsHandle overlapped = try_get_body_handle(get_body_key(hit.mBodyID2));
            if (overlapped.is_valid())
                out_overlaps.push_back(overlapped);
        }
    }

    bool JoltPhysicsBackend::raycast(
        const tbx::RaycastQuery& raycast_query,
        const std::vector<tbx::PhysicsHandle>& ignored,
        tbx::PhysicsRaycastHit& out_hit) const
    {
        out_hit = {};
        if (!_is_ready)
            return false;

        const tbx::Vec3 ray_direction =
            get_safe_normalized(raycast_query.ray.direction, tbx::Vec3(0.0F, 0.0F, -1.0F));
        const float max_distance = std::max(0.0F, raycast_query.max_distance);
        if (max_distance <= 0.0F)
            return false;

        const JPH::RRayCast ray = JPH::RRayCast(
            to_jolt_rvec3(raycast_query.ray.origin),
            to_jolt_vec3(ray_direction * max_distance));

        JPH::IgnoreMultipleBodiesFilter ignore_filter = {};
        ignore_filter.Reserve(static_cast<JPH::uint>(ignored.size()));
        for (const auto& ignored_handle : ignored)
        {
            const auto ignored_it = _bodies.find(ignored_handle.id);
            if (ignored_it != _bodies.end())
                ignore_filter.IgnoreBody(ignored_it->second.body_id);
        }

        const auto& narrow_phase_query = _physics_system.GetNarrowPhaseQuery();
        JPH::RayCastResult ray_hit = {};
        const bool has_hit = narrow_phase_query.CastRay(ray, ray_hit, {}, {}, ignore_filter);

        out_hit.has_hit = has_hit;
        if (!has_hit)
            return true;

        out_hit.rigidbody = try_get_body_handle(get_body_key(ray_hit.mBodyID));
        out_hit.hit_position = to_tbx_vec3_from_rvec3(ray.GetPointOnRay(ray_hit.mFraction));
        return true;
    }

    tbx::PhysicsHandle JoltPhysicsBackend::store_shape(
        JPH::RefConst<JPH::Shape> shape, bool is_sensor)
    {
        if (!shape)
            return {};

        const tbx::PhysicsHandle handle = tbx::PhysicsHandle(tbx::Uuid::generate());
        _shapes[handle.id] = JoltShapeResource {.shape = shape, .is_sensor = is_sensor};
        _pending_shape_id = handle.id;
        return handle;
    }

    JPH::RefConst<JPH::Shape> JoltPhysicsBackend::take_pending_shape(
        bool& out_is_sensor, tbx::Uuid& out_shape_id)
    {
        out_is_sensor = false;
        out_shape_id = {};
        if (!_pending_shape_id.is_valid())
            return nullptr;

        const auto shape_it = _shapes.find(_pending_shape_id);
        _pending_shape_id = {};
        if (shape_it == _shapes.end())
            return nullptr;

        out_is_sensor = shape_it->second.is_sensor;
        out_shape_id = shape_it->first;
        return shape_it->second.shape;
    }

    tbx::PhysicsHandle JoltPhysicsBackend::create_collider(
        const tbx::Collider& collider, const tbx::Mesh& mesh)
    {
        if (!_is_ready)
            return {};

        return store_shape(build_collider_shape(collider, mesh), false);
    }

    tbx::PhysicsHandle JoltPhysicsBackend::create_trigger(
        const tbx::Trigger& trigger, const tbx::Mesh& mesh)
    {
        if (!_is_ready)
            return {};

        return store_shape(build_trigger_shape(trigger, mesh), true);
    }

    tbx::PhysicsHandle JoltPhysicsBackend::create_rigidbody(
        tbx::Transform transform, tbx::Rigidbody rigidbody)
    {
        if (!_is_ready)
            return {};

        bool is_sensor = false;
        tbx::Uuid shape_id = {};
        JPH::RefConst<JPH::Shape> shape = take_pending_shape(is_sensor, shape_id);
        // Dynamic bodies use no shape by default; give a unit box so the body is still valid.
        if (!shape)
            shape = make_box_shape(tbx::Vec3(0.5F, 0.5F, 0.5F));

        const bool physics_driven = rigidbody.is_valid();
        const auto object_layer =
            physics_driven ? get_moving_object_layer() : get_static_object_layer();
        auto body_settings = JPH::BodyCreationSettings(
            shape,
            to_jolt_rvec3(transform.position),
            to_jolt_quat(transform.rotation),
            get_motion_type(rigidbody, physics_driven),
            object_layer);
        body_settings.mIsSensor = is_sensor;

        if (physics_driven)
        {
            apply_dynamic_body_settings(
                rigidbody,
                is_sensor,
                _max_linear_velocity,
                _max_angular_velocity,
                body_settings);
        }

        auto activation = JPH::EActivation::DontActivate;
        if (physics_driven && !rigidbody.is_kinematic)
            activation = JPH::EActivation::Activate;

        const JPH::BodyID body_id =
            _physics_system.GetBodyInterface().CreateAndAddBody(body_settings, activation);
        if (body_id.IsInvalid())
            return {};

        const tbx::PhysicsHandle handle = tbx::PhysicsHandle(tbx::Uuid::generate());
        _bodies[handle.id] = JoltBodyResource {
            .body_id = body_id,
            .shape_id = shape_id,
            .is_sensor = is_sensor,
        };
        _body_by_key[get_body_key(body_id)] = handle.id;
        return handle;
    }

    void JoltPhysicsBackend::destroy(const tbx::PhysicsHandle& physics_handle)
    {
        if (const auto body_it = _bodies.find(physics_handle.id); body_it != _bodies.end())
        {
            auto& body_interface = _physics_system.GetBodyInterface();
            const JPH::BodyID body_id = body_it->second.body_id;
            if (body_interface.IsAdded(body_id))
            {
                body_interface.RemoveBody(body_id);
                body_interface.DestroyBody(body_id);
            }

            _contact_listener.remove_body(get_body_key(body_id));
            _body_by_key.erase(get_body_key(body_id));
            _bodies.erase(body_it);
            return;
        }

        _shapes.erase(physics_handle.id);
        if (_pending_shape_id == physics_handle.id)
            _pending_shape_id = {};
    }

    std::vector<tbx::Vec3>& JoltPhysicsBackend::get_debug_shape(
        tbx::PhysicsHandle physics_handle) const
    {
        _debug_shape.clear();

        tbx::Uuid shape_id = physics_handle.id;
        if (const auto body_it = _bodies.find(physics_handle.id); body_it != _bodies.end())
            shape_id = body_it->second.shape_id;

        const auto shape_it = _shapes.find(shape_id);
        if (shape_it == _shapes.end() || shape_it->second.shape == nullptr)
            return _debug_shape;

        // Stream the cooked shape's debug triangles in shape-local space at unit scale — any entity
        // scale was baked into the shape's source points at creation.
        const JPH::Shape& shape = *shape_it->second.shape;
        JPH::Shape::GetTrianglesContext context = {};
        shape.GetTrianglesStart(
            context,
            JPH::AABox::sBiggest(),
            JPH::Vec3::sZero(),
            JPH::Quat::sIdentity(),
            JPH::Vec3::sReplicate(1.0F));

        constexpr int TRIANGLE_BATCH = 256; // >= Jolt's cGetTrianglesMinTrianglesRequested
        auto batch = std::vector<JPH::Float3>(static_cast<size>(TRIANGLE_BATCH) * 3U);
        for (;;)
        {
            const int triangle_count =
                shape.GetTrianglesNext(context, TRIANGLE_BATCH, batch.data());
            if (triangle_count <= 0)
                break;

            for (int vertex = 0; vertex < triangle_count * 3; ++vertex)
            {
                const auto& point = batch[static_cast<size>(vertex)];
                _debug_shape.emplace_back(point.x, point.y, point.z);
            }
        }
        return _debug_shape;
    }

    tbx::PhysicsHandle JoltPhysicsBackend::try_get_body_handle(std::uint32_t body_key) const
    {
        const auto it = _body_by_key.find(body_key);
        if (it == _body_by_key.end())
            return {};

        return tbx::PhysicsHandle(it->second);
    }

    void JoltPhysicsBackend::clear_resources()
    {
        auto& body_interface = _physics_system.GetBodyInterface();
        for (const auto& body_entry : _bodies)
        {
            const JPH::BodyID body_id = body_entry.second.body_id;
            if (body_interface.IsAdded(body_id))
            {
                body_interface.RemoveBody(body_id);
                body_interface.DestroyBody(body_id);
            }
        }

        _bodies.clear();
        _body_by_key.clear();
        _shapes.clear();
        _pending_shape_id = {};
        _contact_listener.clear();
    }
}
