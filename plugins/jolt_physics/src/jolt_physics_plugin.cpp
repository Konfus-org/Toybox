#include "jolt_physics_plugin.h"
#include "tbx/app/application.h"
#include "tbx/debugging/macros.h"
#include "tbx/math/transform.h"
#include "tbx/physics/collider.h"
#include "tbx/physics/physics.h"
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/IssueReporting.h>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/RegisterTypes.h>
#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>
#include <unordered_set>

namespace tbx::plugins
{
    namespace
    {
        static constexpr JPH::ObjectLayer object_layer_static = 0;
        static constexpr JPH::ObjectLayer object_layer_moving = 1;
        static constexpr JPH::BroadPhaseLayer broad_phase_layer_static = JPH::BroadPhaseLayer(0);
        static constexpr JPH::BroadPhaseLayer broad_phase_layer_moving = JPH::BroadPhaseLayer(1);
        static constexpr std::uint32_t broad_phase_layer_count = 2;

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
            std::vsnprintf(
                message.data(),
                static_cast<std::size_t>(required_chars) + 1U,
                fmt,
                args);
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

        static JPH::Vec3 to_jolt_vec3(const Vec3& value)
        {
            return JPH::Vec3(value.x, value.y, value.z);
        }

        static JPH::RVec3 to_jolt_rvec3(const Vec3& value)
        {
            return JPH::RVec3(value.x, value.y, value.z);
        }

        static JPH::Quat to_jolt_quat(const Quat& value)
        {
            return JPH::Quat(value.x, value.y, value.z, value.w);
        }

        static Vec3 to_tbx_vec3(JPH::Vec3Arg value)
        {
            return Vec3(value.GetX(), value.GetY(), value.GetZ());
        }

        static Vec3 to_tbx_vec3_from_rvec3(JPH::RVec3Arg value)
        {
            return Vec3(
                static_cast<float>(value.GetX()),
                static_cast<float>(value.GetY()),
                static_cast<float>(value.GetZ()));
        }

        static Quat to_tbx_quat(JPH::QuatArg value)
        {
            return Quat(value.GetW(), value.GetX(), value.GetY(), value.GetZ());
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

        static bool has_any_collider(const Entity& entity)
        {
            return entity.has_component<SphereCollider>() || entity.has_component<CapsuleCollider>()
                   || entity.has_component<CubeCollider>() || entity.has_component<MeshCollider>();
        }

        static JPH::RefConst<JPH::Shape> create_shape_for_entity(const Entity& entity)
        {
            if (entity.has_component<SphereCollider>())
                return create_sphere_shape(entity.get_component<SphereCollider>());

            if (entity.has_component<CapsuleCollider>())
                return create_capsule_shape(entity.get_component<CapsuleCollider>());

            if (entity.has_component<CubeCollider>())
                return create_box_shape(entity.get_component<CubeCollider>());

            if (entity.has_component<MeshCollider>())
                return new JPH::BoxShape(JPH::Vec3(0.5F, 0.5F, 0.5F));

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
    }

    void JoltPhysicsPlugin::on_attach(IPluginHost&)
    {
        if (!acquire_jolt_runtime())
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

        auto& settings = get_host().get_settings().physics;

        auto max_bodies = std::max<std::uint32_t>(1U, settings.max_body_count.value);
        auto max_pairs = std::max<std::uint32_t>(1U, settings.max_body_pairs.value);
        auto max_constraints = std::max<std::uint32_t>(1U, settings.max_contact_constraints.value);

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
        _step_accumulator_seconds = 0.0;
        _is_ready = true;
    }

    void JoltPhysicsPlugin::on_detach()
    {
        clear_bodies();
        _job_system.reset();
        _temp_allocator.reset();
        _is_ready = false;
        _step_accumulator_seconds = 0.0;
        release_jolt_runtime();
    }

    void JoltPhysicsPlugin::on_update(const DeltaTime& dt)
    {
        if (!_is_ready || !_temp_allocator || !_job_system)
            return;

        auto& physics_settings = get_host().get_settings().physics;
        float fixed_step = std::max(0.0001F, physics_settings.fixed_time_step_seconds.value);
        int max_sub_steps = std::max(1, static_cast<int>(physics_settings.max_sub_steps.value));

        apply_world_settings();
        sync_entities_to_world(fixed_step);

        _step_accumulator_seconds += std::max(0.0, dt.seconds);

        int sub_step_count = 0;
        while (_step_accumulator_seconds >= fixed_step && sub_step_count < max_sub_steps)
        {
            JPH::EPhysicsUpdateError update_error =
                _physics_system.Update(fixed_step, 1, _temp_allocator.get(), _job_system.get());
            if (update_error != JPH::EPhysicsUpdateError::None)
                TBX_TRACE_WARNING(
                    "Jolt physics update reported error flags: {}",
                    static_cast<std::uint32_t>(update_error));

            _step_accumulator_seconds -= fixed_step;
            ++sub_step_count;
        }

        double max_accumulator_seconds =
            static_cast<double>(fixed_step) * static_cast<double>(std::max(1, max_sub_steps));
        if (_step_accumulator_seconds > max_accumulator_seconds)
            _step_accumulator_seconds = max_accumulator_seconds;

        sync_world_to_entities();
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

        auto entities = registry.get_with<Transform>();
        for (auto& entity : entities)
        {
            Uuid entity_id = entity.get_id();

            auto& transform = entity.get_component<Transform>();
            const bool has_physics_component = entity.has_component<Physics>();
            const bool has_collider = has_any_collider(entity);
            if (!has_physics_component && !has_collider)
                continue;

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

                _bodies_by_entity.erase(body_it);
                body_it = _bodies_by_entity.end();
            }

            if (body_it == _bodies_by_entity.end())
            {
                JPH::RefConst<JPH::Shape> shape = create_shape_for_entity(entity);
                if (!shape)
                    continue;

                auto object_layer = is_physics_driven ? object_layer_moving : object_layer_static;
                auto motion_type =
                    is_physics_driven ? get_motion_type(*physics) : JPH::EMotionType::Static;

                auto body_settings = JPH::BodyCreationSettings(
                    shape,
                    to_jolt_rvec3(transform.position),
                    to_jolt_quat(transform.rotation),
                    motion_type,
                    object_layer);

                if (is_physics_driven)
                {
                    body_settings.mIsSensor = physics->is_sensor;
                    body_settings.mAllowSleeping = physics->is_sleep_enabled;
                    body_settings.mFriction = physics->friction;
                    body_settings.mRestitution = physics->restitution;
                    body_settings.mLinearDamping = physics->default_linear_damping;
                    body_settings.mAngularDamping = physics->default_angular_damping;
                    body_settings.mLinearVelocity = to_jolt_vec3(physics->linear_velocity);
                    body_settings.mAngularVelocity = to_jolt_vec3(physics->angular_velocity);
                    body_settings.mGravityFactor = physics->is_gravity_enabled ? 1.0F : 0.0F;
                    body_settings.mMaxLinearVelocity =
                        std::max(0.0F, get_host().get_settings().physics.max_linear_velocity.value);
                    body_settings.mMaxAngularVelocity = std::max(
                        0.0F,
                        get_host().get_settings().physics.max_angular_velocity.value);
                    body_settings.mOverrideMassProperties =
                        JPH::EOverrideMassProperties::CalculateInertia;
                    body_settings.mMassPropertiesOverride.mMass = physics->mass;
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

                _bodies_by_entity[entity_id] = BodyRecord {body_id, is_physics_driven};
                continue;
            }

            JPH::BodyID body_id = body_it->second.body_id;
            if (!body_interface.IsAdded(body_id))
                continue;

            if (!is_physics_driven)
            {
                body_interface.SetPositionAndRotation(
                    body_id,
                    to_jolt_rvec3(transform.position),
                    to_jolt_quat(transform.rotation),
                    JPH::EActivation::DontActivate);
                continue;
            }

            if (physics->is_kinematic)
            {
                body_interface.MoveKinematic(
                    body_id,
                    to_jolt_rvec3(transform.position),
                    to_jolt_quat(transform.rotation),
                    std::max(0.0001F, dt_seconds));

                body_interface.SetLinearVelocity(body_id, to_jolt_vec3(physics->linear_velocity));
                body_interface.SetAngularVelocity(body_id, to_jolt_vec3(physics->angular_velocity));
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

            _bodies_by_entity.erase(body_it);
        }
    }

    void JoltPhysicsPlugin::sync_world_to_entities()
    {
        auto& registry = get_host().get_entity_registry();
        auto& body_interface = _physics_system.GetBodyInterface();

        for (const auto& body_entry : _bodies_by_entity)
        {
            const Uuid& entity_id = body_entry.first;
            const JPH::BodyID& body_id = body_entry.second.body_id;

            if (!registry.has<Physics>(entity_id) || !registry.has<Transform>(entity_id))
                continue;

            auto& physics = registry.get_with<Physics>(entity_id);
            auto& transform = registry.get_with<Transform>(entity_id);

            if (!body_interface.IsAdded(body_id))
                continue;

            physics.linear_velocity = to_tbx_vec3(body_interface.GetLinearVelocity(body_id));
            physics.angular_velocity = to_tbx_vec3(body_interface.GetAngularVelocity(body_id));

            if (physics.is_kinematic)
                continue;

            transform.position = to_tbx_vec3_from_rvec3(body_interface.GetPosition(body_id));
            transform.rotation = to_tbx_quat(body_interface.GetRotation(body_id));
        }
    }
}
