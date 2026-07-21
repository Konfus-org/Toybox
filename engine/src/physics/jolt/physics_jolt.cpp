#include "tbx/physics/physics.h"
#include "tbx/app.h"
#include "tbx/core/log.h"
#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace tbx::physics
{
    //// LAYERS ////

    static constexpr JPH::ObjectLayer LAYER_NON_MOVING = 0;
    static constexpr JPH::ObjectLayer LAYER_MOVING = 1;
    static constexpr uint LAYER_COUNT = 2;

    /// @brief
    /// Purpose: Maps object layers onto Jolt broadphase layers (1:1 — two layers is plenty).
    class BroadPhaseLayers final : public JPH::BroadPhaseLayerInterface
    {
      public:
        JPH::uint GetNumBroadPhaseLayers() const override
        {
            return LAYER_COUNT;
        }

        JPH::BroadPhaseLayer GetBroadPhaseLayer(const JPH::ObjectLayer layer) const override
        {
            return JPH::BroadPhaseLayer(static_cast<JPH::BroadPhaseLayer::Type>(layer));
        }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        const char* GetBroadPhaseLayerName(const JPH::BroadPhaseLayer layer) const override
        {
            return layer == JPH::BroadPhaseLayer(0) ? "NON_MOVING" : "MOVING";
        }
#endif
    };

    /// @brief
    /// Purpose: Static scenery never tests against itself.
    class ObjectVsBroadPhaseFilter final : public JPH::ObjectVsBroadPhaseLayerFilter
    {
      public:
        bool ShouldCollide(const JPH::ObjectLayer layer, const JPH::BroadPhaseLayer broadphase)
            const override
        {
            return layer == LAYER_MOVING || broadphase == JPH::BroadPhaseLayer(LAYER_MOVING);
        }
    };

    /// @brief
    /// Purpose: Same rule between object layers: at least one side must move.
    class ObjectLayerFilter final : public JPH::ObjectLayerPairFilter
    {
      public:
        bool ShouldCollide(const JPH::ObjectLayer a, const JPH::ObjectLayer b) const override
        {
            return a == LAYER_MOVING || b == LAYER_MOVING;
        }
    };

    /// @brief
    /// Purpose: Collects new contacts during the (multi-threaded) step; drained after Update
    /// into the pump-delivered collision signal.
    class ContactCollector final : public JPH::ContactListener
    {
      public:
        void OnContactAdded(
            const JPH::Body& body_a,
            const JPH::Body& body_b,
            const JPH::ContactManifold&,
            JPH::ContactSettings&) override
        {
            const std::scoped_lock lock(_mutex);
            _pairs.push_back(
                {static_cast<uint32>(body_a.GetUserData()),
                 static_cast<uint32>(body_b.GetUserData())});
        }

        std::vector<std::pair<uint32, uint32>> drain()
        {
            const std::scoped_lock lock(_mutex);
            auto drained = std::move(_pairs);
            _pairs.clear();
            return drained;
        }

      private:
        std::mutex _mutex;
        std::vector<std::pair<uint32, uint32>> _pairs;
    };

    //// STATE ////

    /// @brief
    /// Purpose: The whole simulation, torn down by reset() and rebuilt lazily by step().
    struct PhysicsState
    {
        JPH::TempAllocatorImpl temp {10 * 1024 * 1024};
        JPH::JobSystemThreadPool jolt_jobs {
            JPH::cMaxPhysicsJobs,
            JPH::cMaxPhysicsBarriers,
            static_cast<int>(std::max(1u, std::thread::hardware_concurrency() - 1))};
        BroadPhaseLayers broadphase_layers = {};
        ObjectVsBroadPhaseFilter object_vs_broadphase = {};
        ObjectLayerFilter object_pairs = {};
        ContactCollector contacts = {};
        JPH::PhysicsSystem system = {};
        std::unordered_map<uint32, JPH::BodyID> bodies_by_toy;

        PhysicsState()
        {
            system.Init(
                4096,
                0,
                4096,
                1024,
                broadphase_layers,
                object_vs_broadphase,
                object_pairs);
            system.SetContactListener(&contacts);
        }
    };

    static std::unique_ptr<PhysicsState> g_physics = {};

    static PhysicsState& ensure_simulation()
    {
        static bool g_jolt_registered = false;
        if (!g_jolt_registered)
        {
            g_jolt_registered = true;
            JPH::RegisterDefaultAllocator();
            JPH::Factory::sInstance = new JPH::Factory();
            JPH::RegisterTypes();
        }
        if (!g_physics)
            g_physics = std::make_unique<PhysicsState>();
        return *g_physics;
    }

    //// CONVERSIONS ////

    static JPH::RVec3 to_jolt(const Vec3& v)
    {
        return {v.x, v.y, v.z};
    }

    static JPH::Quat to_jolt(const Quat& q)
    {
        return {q.x, q.y, q.z, q.w};
    }

    static Vec3 to_engine(const JPH::RVec3& v)
    {
        return {v.GetX(), v.GetY(), v.GetZ()};
    }

    static Quat to_engine(const JPH::Quat& q)
    {
        return {q.GetW(), q.GetX(), q.GetY(), q.GetZ()};
    }

    /// @brief
    /// Purpose: Maps the shared Shape vocabulary onto Jolt shapes.
    static JPH::ShapeRefC make_shape(const Collider& collider)
    {
        switch (collider.shape)
        {
            case Shape::SPHERE:
                return new JPH::SphereShape(collider.radius);
            case Shape::CAPSULE:
                return new JPH::CapsuleShape(collider.height * 0.5f, collider.radius);
            case Shape::BOX:
                break;
        }
        return new JPH::BoxShape(to_jolt(collider.half_extents));
    }

    //// BOUNDARY ////

    void reset()
    {
        g_physics.reset();
    }

    void step(Sandbox& sandbox, Events& events, const float fixed_delta_time)
    {
        register_builtin_blocks();
        PhysicsState& physics = ensure_simulation();
        JPH::BodyInterface& bodies = physics.system.GetBodyInterface();
        auto& registry = sandbox.get_registry();

        // Mirror collider toys into the simulation (created on first sight).
        for (const auto [entity, collider] : registry.view<Collider>().each())
        {
            if (!registry.get<ToyHandle>(entity).is_enabled)
                continue;
            const auto key = static_cast<uint32>(entity);
            const auto* rigid_body = registry.try_get<RigidBody>(entity);
            const auto& transform = registry.get_or_emplace<Transform>(entity);
            const auto existing = physics.bodies_by_toy.find(key);
            if (existing == physics.bodies_by_toy.end())
            {
                auto settings = JPH::BodyCreationSettings(
                    make_shape(collider),
                    to_jolt(transform.position),
                    to_jolt(transform.rotation),
                    rigid_body ? (rigid_body->is_kinematic ? JPH::EMotionType::Kinematic
                                                           : JPH::EMotionType::Dynamic)
                               : JPH::EMotionType::Static,
                    rigid_body ? LAYER_MOVING : LAYER_NON_MOVING);
                settings.mUserData = key;
                if (rigid_body && !rigid_body->is_kinematic)
                {
                    settings.mOverrideMassProperties =
                        JPH::EOverrideMassProperties::CalculateInertia;
                    settings.mMassPropertiesOverride.mMass = rigid_body->mass;
                }
                physics.bodies_by_toy[key] = bodies.CreateAndAddBody(
                    settings,
                    rigid_body ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
            }
            else if (rigid_body && rigid_body->is_kinematic)
            {
                bodies.MoveKinematic(
                    existing->second,
                    to_jolt(transform.position),
                    to_jolt(transform.rotation),
                    fixed_delta_time);
            }
        }

        // Bodies whose toys despawned leave the simulation.
        for (auto it = physics.bodies_by_toy.begin(); it != physics.bodies_by_toy.end();)
        {
            if (!registry.valid(static_cast<ToyId>(it->first)))
            {
                bodies.RemoveBody(it->second);
                bodies.DestroyBody(it->second);
                it = physics.bodies_by_toy.erase(it);
            }
            else
                ++it;
        }

        physics.system.Update(fixed_delta_time, 1, &physics.temp, &physics.jolt_jobs);

        // Dynamic poses write back into Transforms (the body is the authority while simulated).
        for (const auto& [key, body_id] : physics.bodies_by_toy)
        {
            if (bodies.GetMotionType(body_id) != JPH::EMotionType::Dynamic)
                continue;
            auto& transform = registry.get<Transform>(static_cast<ToyId>(key));
            JPH::RVec3 position = {};
            JPH::Quat rotation = {};
            bodies.GetPositionAndRotation(body_id, position, rotation);
            transform.position = to_engine(position);
            transform.rotation = to_engine(rotation);
        }

        for (const auto& [toy_a, toy_b] : physics.contacts.drain())
            events.collision.emit({.toy_a = toy_a, .toy_b = toy_b});
    }

    std::optional<RaycastHit> raycast(
        const Vec3& origin,
        const Vec3& direction,
        const float max_distance)
    {
        if (!g_physics)
            return {};
        const Vec3 normalized = math::normalize(direction);
        const auto ray = JPH::RRayCast(
            to_jolt(origin),
            to_jolt(normalized * max_distance));
        auto hit = JPH::RayCastResult {};
        if (!g_physics->system.GetNarrowPhaseQuery().CastRay(ray, hit))
            return {};
        const float distance = hit.mFraction * max_distance;
        const auto toy = static_cast<ToyId>(
            g_physics->system.GetBodyInterface().GetUserData(hit.mBodyID));
        return RaycastHit {
            .toy = toy,
            .position = origin + normalized * distance,
            .distance = distance};
    }
}
