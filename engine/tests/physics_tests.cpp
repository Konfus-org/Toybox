#include "tbx/app.h"
#include "tbx/physics/physics.h"
#include "runtime_state.h"
#include "tbx/assets/assets.h"
#include <gtest/gtest.h>

namespace tbx
{

    static constexpr float STEP = 1.0f / 60.0f;

    TEST(Physics, FallingCubeRestsOnFloor)
    {
        // Arrange
        auto toybox = Runtime();
        internal::RuntimeState& runtime = *toybox.state;
        Sandbox& sandbox = runtime.sandbox;
        sandbox.add("Floor")
            .with(Transform {.position = Vec3(0.0f, -0.5f, 0.0f)})
            .with(Collider {.half_extents = Vec3(20.0f, 0.5f, 20.0f)});
        Toy cube = sandbox.add("Cube")
                       .with(Transform {.position = Vec3(0.0f, 5.0f, 0.0f)})
                       .with(Collider {})
                       .with(RigidBody {});

        // Act: ~3 simulated seconds — plenty to fall from 5 units and settle.
        for (int i = 0; i < 180; ++i)
            internal::update_physics(runtime.physics, sandbox, runtime.assets, runtime.events, STEP);

        // Assert: resting with its half-extent (0.5) above the floor top (y = 0).
        const float resting_y = cube.add<Transform>().position.y;
        EXPECT_NEAR(resting_y, 0.5f, 0.1f);
    }

    TEST(Physics, StaticColliderNeverMoves)
    {
        // Arrange
        auto toybox = Runtime();
        internal::RuntimeState& runtime = *toybox.state;
        Sandbox& sandbox = runtime.sandbox;
        Toy wall = sandbox.add("Wall")
                       .with(Transform {.position = Vec3(3.0f, 4.0f, 0.0f)})
                       .with(Collider {});

        // Act
        for (int i = 0; i < 60; ++i)
            internal::update_physics(runtime.physics, sandbox, runtime.assets, runtime.events, STEP);

        // Assert: no RigidBody means scenery — gravity does not apply.
        EXPECT_EQ(wall.add<Transform>().position, Vec3(3.0f, 4.0f, 0.0f));
    }

    TEST(Physics, CollisionEventReachesSubscribersThroughThePump)
    {
        // Arrange
        auto toybox = Runtime();
        internal::RuntimeState& runtime = *toybox.state;
        Sandbox& sandbox = runtime.sandbox;
        auto collisions = std::vector<std::pair<uint32, uint32>>();
        runtime.events.signal<CollisionEvent>().subscribe(
            &collisions,
            [&collisions](const CollisionEvent& hit)
            { collisions.push_back({hit.toy_a, hit.toy_b}); });
        sandbox.add("Floor")
            .with(Transform {.position = Vec3(0.0f, -0.5f, 0.0f)})
            .with(Collider {.half_extents = Vec3(20.0f, 0.5f, 20.0f)});
        Toy cube = sandbox.add("Cube")
                       .with(Transform {.position = Vec3(0.0f, 2.0f, 0.0f)})
                       .with(Collider {})
                       .with(RigidBody {});

        // Act: fall to impact, then drain — collision delivery happens at the pump.
        for (int i = 0; i < 120; ++i)
            internal::update_physics(runtime.physics, sandbox, runtime.assets, runtime.events, STEP);
        const auto before_drain = collisions.size();
        internal::update_events(runtime.events);

        // Assert
        EXPECT_EQ(before_drain, 0u);
        ASSERT_FALSE(collisions.empty());
        const auto cube_id = static_cast<uint32>(cube.get_id());
        EXPECT_TRUE(
            collisions[0].first == cube_id || collisions[0].second == cube_id);
    }

    TEST(Physics, RigidBodyCollidedComponentSignalFiresWithTheOtherToy)
    {
        // Arrange: the cube's RigidBody owns a `collided` signal that scripts connect to directly.
        auto toybox = Runtime();
        internal::RuntimeState& runtime = *toybox.state;
        Sandbox& sandbox = runtime.sandbox;
        sandbox.add("Floor")
            .with(Transform {.position = Vec3(0.0f, -0.5f, 0.0f)})
            .with(Collider {.half_extents = Vec3(20.0f, 0.5f, 20.0f)});
        Toy cube = sandbox.add("Cube")
                       .with(Transform {.position = Vec3(0.0f, 2.0f, 0.0f)})
                       .with(Collider {})
                       .with(RigidBody {});
        auto struck = std::vector<std::string>();
        cube.get<RigidBody>()->collided.connect(
            [&struck](const Toy& other) { struck.push_back(other.get_name()); });

        // Act: fall to impact — component signals dispatch inline during the step (no pump needed).
        for (int i = 0; i < 120; ++i)
            internal::update_physics(runtime.physics, sandbox, runtime.assets, runtime.events, STEP);

        // Assert: the cube's own event fired, carrying the floor toy.
        ASSERT_FALSE(struck.empty());
        EXPECT_EQ(struck[0], "Floor");
    }

    TEST(Physics, RigidBodyCollidedIsSilentWithoutAnySubscriber)
    {
        // Arrange
        auto toybox = Runtime();
        internal::RuntimeState& runtime = *toybox.state;
        Sandbox& sandbox = runtime.sandbox;
        sandbox.add("Floor")
            .with(Transform {.position = Vec3(0.0f, -0.5f, 0.0f)})
            .with(Collider {.half_extents = Vec3(20.0f, 0.5f, 20.0f)});
        sandbox.add("Cube")
            .with(Transform {.position = Vec3(0.0f, 2.0f, 0.0f)})
            .with(Collider {})
            .with(RigidBody {});

        // Act + Assert: a collision with nothing connected to collided must not crash.
        for (int i = 0; i < 120; ++i)
            internal::update_physics(runtime.physics, sandbox, runtime.assets, runtime.events, STEP);
        SUCCEED();
    }

    TEST(Physics, RaycastHitsAndMisses)
    {
        // Arrange
        auto toybox = Runtime();
        internal::RuntimeState& runtime = *toybox.state;
        Sandbox& sandbox = runtime.sandbox;
        Toy target = sandbox.add("Target")
                         .with(Transform {.position = Vec3(0.0f, 0.0f, -5.0f)})
                         .with(Collider {});
        internal::update_physics(runtime.physics, sandbox, runtime.assets, runtime.events, STEP); // mirror the body in

        // Act
        const auto hit =
            internal::raycast(runtime.physics, Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f), 50.0f);
        const auto miss =
            internal::raycast(runtime.physics, Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), 50.0f);

        // Assert: the front face sits at z = -4.5, 4.5 units down the ray.
        ASSERT_TRUE(hit.has_value());
        EXPECT_EQ(hit->toy, target.get_id());
        EXPECT_NEAR(hit->distance, 4.5f, 0.05f);
        EXPECT_FALSE(miss.has_value());
    }
}
