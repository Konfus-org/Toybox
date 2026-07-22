#include "jolt_first.h"
#include "tbx/runtime.h"
#include "tbx/app.h"
#include "tbx/debug/log.h"
#include "tbx/reflection/reflection.h"
#include "tbx/physics/physics.h"
#include "tbx/events/events.h"
#include "tbx/ecs/sandbox.h"
#include "tbx/physics/rigid_body.h"
#include "tbx/physics/collider.h"
#include "tbx/gpu/renderer.h"
#include "tbx/assets/assets.h"
#include "tbx/math/transform.h"
#include "tbx/assets/builtin.h"
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>


namespace tbx
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
    /// Purpose: The whole simulation behind the boundary: Jolt world, filters, mirrored
    /// bodies. PhysicsState owns exactly one, built lazily on the first update.
    struct PhysicsState::Simulation
    {
        JPH::TempAllocatorImpl temp;
        JPH::JobSystemThreadPool jolt_jobs;
        BroadPhaseLayers broadphase_layers = {};
        ObjectVsBroadPhaseFilter object_vs_broadphase = {};
        ObjectLayerFilter object_pairs = {};
        ContactCollector contacts = {};
        JPH::PhysicsSystem system = {};
        std::unordered_map<uint32, JPH::BodyID> bodies_by_toy;

        Simulation() :
            temp(10 * 1024 * 1024),
            jolt_jobs(
                JPH::cMaxPhysicsJobs,
                JPH::cMaxPhysicsBarriers,
                static_cast<int>(std::max(1u, std::thread::hardware_concurrency() - 1)))
        {
            system.Init(4096, 0, 4096, 1024, broadphase_layers, object_vs_broadphase, object_pairs);
            system.SetContactListener(&contacts);
        }
    };

    PhysicsState::PhysicsState() = default;
    PhysicsState::~PhysicsState() = default;

    static PhysicsState::Simulation& ensure_simulation(PhysicsState& state)
    {
        // Jolt's allocator/factory/type registration is inherently process-global; it is set
        // up exactly once and deliberately lives (leaks) for the process lifetime - Runtimes
        // come and go underneath it.
        static bool g_jolt_registered = false;
        if (!g_jolt_registered)
        {
            g_jolt_registered = true;
            JPH::RegisterDefaultAllocator();
            JPH::Factory::sInstance = new JPH::Factory();
            JPH::RegisterTypes();
        }
        if (!state.simulation)
            state.simulation = std::make_unique<PhysicsState::Simulation>();
        return *state.simulation;
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
    /// Purpose: The scale-sized box every mesh-collider fallback path shares (Jolt idiom:
    /// ShapeRefC adopts the intrusively-refcounted new).
    static JPH::ShapeRefC make_fallback_box(const Vec3& scale)
    {
        return new JPH::BoxShape(to_jolt(scale * 0.5f));
    }

    /// @brief
    /// Purpose: A concave mesh shape from a Renderer's geometry: imported model triangles
    /// (scaled by the transform) or an analytic stand-in for the builtin primitives.
    static JPH::ShapeRefC make_mesh_shape(
        AssetsState& assets,
        EventsState& events,
        Toy toy,
        const Vec3& scale)
    {
        const auto* renderer = toy.try_block<Renderer>();
        if (!renderer)
        {
            TBX_WARN("Shape::MESH collider without a Renderer block; falling back to a box");
            return make_fallback_box(scale);
        }

        // Builtin primitives get their exact analytic shapes.
        if (!renderer->model.is_set() || renderer->model.id == Builtin::CUBE.id)
            return make_fallback_box(scale);
        if (renderer->model.id == Builtin::SPHERE.id)
            return new JPH::SphereShape(std::max({scale.x, scale.y, scale.z}) * 0.5f);
        if (renderer->model.id == Builtin::PLANE.id)
            return new JPH::BoxShape(
                JPH::Vec3(scale.x * 0.5f, std::max(scale.y * 0.01f, 0.02f), scale.z * 0.5f));

        if (const auto model = load_asset_now(assets, events, renderer->model))
        {
            // Interleaved position(3)+normal(3)+uv(2) triangle list from the importer.
            const auto& vertices = model->get().vertices;
            constexpr size STRIDE = 8;
            auto triangles = JPH::TriangleList();
            for (size at = 0; at + STRIDE * 3 <= vertices.size(); at += STRIDE * 3)
            {
                JPH::Float3 corners[3] = {};
                for (int corner = 0; corner < 3; ++corner)
                {
                    const size base = at + static_cast<size>(corner) * STRIDE;
                    corners[corner] = JPH::Float3(
                        vertices[base + 0] * scale.x,
                        vertices[base + 1] * scale.y,
                        vertices[base + 2] * scale.z);
                }
                triangles.push_back(JPH::Triangle(corners[0], corners[1], corners[2]));
            }
            auto settings = JPH::MeshShapeSettings(triangles);
            auto result = settings.Create();
            if (result.IsValid())
                return result.Get();
            TBX_WARN("mesh collider failed ({}); falling back to a box",
                result.GetError().c_str());
        }
        else
            TBX_WARN("mesh collider model unavailable; falling back to a box");
        return make_fallback_box(scale);
    }

    /// @brief
    /// Purpose: Maps the shared Shape vocabulary onto Jolt shapes.
    static JPH::ShapeRefC make_shape(
        AssetsState& assets,
        EventsState& events,
        const Collider& collider,
        Toy toy,
        const Vec3& scale)
    {
        switch (collider.shape)
        {
            case Shape::SPHERE:
                return new JPH::SphereShape(collider.radius);
            case Shape::CAPSULE:
                return new JPH::CapsuleShape(collider.height * 0.5f, collider.radius);
            case Shape::MESH:
                return make_mesh_shape(assets, events, toy, scale);
            case Shape::BOX:
                break;
        }
        return new JPH::BoxShape(to_jolt(collider.half_extents));
    }

    /// @brief
    /// Purpose: One transform in another's space (parent ∘ child) — TRS composition.
    static Transform compose(const Transform& parent, const Transform& child)
    {
        return Transform {
            .position = parent.position + rotate(parent.rotation, parent.scale * child.position),
            .rotation = multiply(parent.rotation, child.rotation),
            .scale = parent.scale * child.scale};
    }

    /// @brief
    /// Purpose: A toy's world transform (local composed up the parent chain) — Jolt bodies
    /// live in world space, so this is what placement reads and write-back inverts.
    static Transform world_pose(Toy toy)
    {
        auto chain = std::vector<Transform>();
        for (auto current = std::optional<Toy>(toy); current && current->is_alive();
             current = current->get_parent())
            chain.push_back(current->get_transform());
        auto world = Transform {};
        for (auto it = chain.rbegin(); it != chain.rend(); ++it)
            world = compose(world, *it);
        return world;
    }

    //// BOUNDARY ////

    void update_physics(
        PhysicsState& state,
        Sandbox& sandbox,
        AssetsState& assets,
        EventsState& events,
        const float fixed_delta_time)
    {
        initialize_reflection();
        PhysicsState::Simulation& physics = ensure_simulation(state);
        // Gravity is a plain field on the state; whatever it says now is what this step uses.
        physics.system.SetGravity(to_jolt(state.gravity));
        JPH::BodyInterface& bodies = physics.system.GetBodyInterface();

        // Mirror collider toys into the simulation (created on first sight).
        sandbox.each<Collider>(
            [&](Toy toy, Collider& collider)
            {
            if (!toy.is_enabled())
                return;
            const auto key = static_cast<uint32>(toy.get_id());
            const auto* rigid_body = toy.try_block<RigidBody>();
            // Jolt simulates in world space; place bodies at the toy's world transform.
            const Transform transform = world_pose(toy);
            const auto existing = physics.bodies_by_toy.find(key);
            if (existing == physics.bodies_by_toy.end())
            {
                auto settings = JPH::BodyCreationSettings(
                    make_shape(assets, events, collider, toy, transform.scale),
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
            });

        // Bodies whose toys despawned leave the simulation.
        for (auto it = physics.bodies_by_toy.begin(); it != physics.bodies_by_toy.end();)
        {
            if (!Toy(sandbox, static_cast<ToyId>(it->first)).is_alive())
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
            auto body_toy = Toy(sandbox, static_cast<ToyId>(key));
            if (!body_toy.is_alive())
                continue;
            JPH::RVec3 position = {};
            JPH::Quat rotation = {};
            bodies.GetPositionAndRotation(body_id, position, rotation);
            const Vec3 world_position = to_engine(position);
            const Quat world_rotation = to_engine(rotation);
            // The body pose is world space; store it back as the toy's LOCAL transform,
            // undoing any parent so a parented dynamic body lands where physics put it.
            auto& local = body_toy.get_transform();
            const auto parent_toy = body_toy.get_parent();
            if (parent_toy)
            {
                const Transform parent = world_pose(*parent_toy);
                const Quat inverse_parent = Quat(
                    parent.rotation.w,
                    -parent.rotation.x,
                    -parent.rotation.y,
                    -parent.rotation.z);
                local.position =
                    rotate(inverse_parent, world_position - parent.position) / parent.scale;
                local.rotation = multiply(inverse_parent, world_rotation);
            }
            else
            {
                local.position = world_position;
                local.rotation = world_rotation;
            }
        }

        for (const auto& [toy_a, toy_b] : physics.contacts.drain())
            events.collision.emit({.toy_a = toy_a, .toy_b = toy_b});
    }

    std::optional<RaycastHit> raycast(
        PhysicsState& state,
        const Vec3& origin,
        const Vec3& direction,
        const float max_distance)
    {
        if (!state.simulation)
            return {};
        PhysicsState::Simulation& physics = *state.simulation;
        const Vec3 normalized = normalize(direction);
        const auto ray = JPH::RRayCast(to_jolt(origin), to_jolt(normalized * max_distance));
        auto hit = JPH::RayCastResult {};
        if (!physics.system.GetNarrowPhaseQuery().CastRay(ray, hit))
            return {};
        const float distance = hit.mFraction * max_distance;
        const auto toy =
            static_cast<ToyId>(physics.system.GetBodyInterface().GetUserData(hit.mBodyID));
        return RaycastHit {
            .toy = toy,
            .position = origin + normalized * distance,
            .distance = distance};
    }
}
