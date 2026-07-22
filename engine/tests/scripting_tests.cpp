#include "tbx/scripting/scripts.h"
#include "tbx/runtime.h"
#include "tbx/math/transform.h"
#include "tbx/physics/rigid_body.h"
#include "tbx/platform/keys.h"
#include "tbx/assets/assets.h"
#include "../src/scripting/builtin_backends.h" // engine-internal: tests wire the VMs directly
#include <gtest/gtest.h>

namespace tbx::tests
{

    static constexpr const char* MOVER_SOURCE = R"(
function start(toy)
    toy:set_name("started")
end
function update(toy, delta_time)
    local position = toy.Transform.position
    toy.Transform.position = { x = position.x + 1.0, y = position.y, z = position.z }
end
)";

    TEST(Scripts, StartRunsOnceAndUpdateMovesToy)
    {
        // Arrange
        auto toybox = Runtime();
        RuntimeState& runtime = *toybox.state;
        Sandbox& sandbox = runtime.sandbox;
        initialize(runtime); // wires the VM backends
        const auto mover = load_source(runtime.scripts, "mover", MOVER_SOURCE);
        ASSERT_TRUE(mover.has_value());
        Toy toy = sandbox.spawn("Grunt").with(Script {.source = *mover});

        // Act
        update_scripts(runtime.scripts, sandbox, runtime.assets, runtime.events, 0.016f);
        update_scripts(runtime.scripts, sandbox, runtime.assets, runtime.events, 0.016f);

        // Assert: start renamed once; update advanced position twice.
        EXPECT_EQ(toy.get_name(), "started");
        EXPECT_EQ(toy.get_block<Transform>().position.x, 2.0f);
    }

    TEST(Scripts, LoadRejectsBadSyntax)
    {
        // Arrange
        auto toybox = Runtime();
        RuntimeState& runtime = *toybox.state;
        Sandbox& sandbox = runtime.sandbox;
        initialize(runtime); // wires the VM backends

        // Act
        const auto result = load_source(runtime.scripts, "broken", "this is not luau ((");

        // Assert
        EXPECT_FALSE(result.has_value());
    }

    TEST(Scripts, ReloadRestartsInstancesAndFiresEvent)
    {
        // Arrange
        auto toybox = Runtime();
        RuntimeState& runtime = *toybox.state;
        Sandbox& sandbox = runtime.sandbox;
        initialize(runtime); // wires the VM backends
        auto reload_count = 0;
        runtime.events.script_reloaded.subscribe(
            &reload_count,
            [&reload_count](const ScriptReloaded&) { ++reload_count; });
        const auto mover = load_source(runtime.scripts, "mover", MOVER_SOURCE);
        ASSERT_TRUE(mover.has_value());
        Toy toy = sandbox.spawn("Grunt").with(Script {.source = *mover});
        update_scripts(runtime.scripts, sandbox, runtime.assets, runtime.events, 0.016f);
        toy.set_name("renamed-by-test");

        // Act: v2 renames differently on start; instance must restart.
        const auto reloaded = reload_source(runtime.scripts, "mover", R"(
function start(toy)
    toy:set_name("restarted")
end
)");
        update_scripts(runtime.scripts, sandbox, runtime.assets, runtime.events, 0.016f);
        update_events(runtime.events);

        // Assert
        ASSERT_TRUE(reloaded.has_value()) << reloaded.error();
        EXPECT_EQ(toy.get_name(), "restarted");
        EXPECT_EQ(reload_count, 1);
    }

    TEST(Scripts, ReloadWithBadSyntaxKeepsOldBehavior)
    {
        // Arrange
        auto toybox = Runtime();
        RuntimeState& runtime = *toybox.state;
        Sandbox& sandbox = runtime.sandbox;
        initialize(runtime); // wires the VM backends
        const auto mover = load_source(runtime.scripts, "mover", MOVER_SOURCE);
        ASSERT_TRUE(mover.has_value());
        Toy toy = sandbox.spawn("Grunt").with(Script {.source = *mover});
        update_scripts(runtime.scripts, sandbox, runtime.assets, runtime.events, 0.016f);

        // Act
        const auto reloaded = reload_source(runtime.scripts, "mover", "broken ((");
        update_scripts(runtime.scripts, sandbox, runtime.assets, runtime.events, 0.016f);

        // Assert: reload failed, old script keeps running.
        EXPECT_FALSE(reloaded.has_value());
        EXPECT_EQ(toy.get_block<Transform>().position.x, 2.0f);
    }

    TEST(Scripts, ScriptCanSpawnAndStickerThroughTbxApi)
    {
        // Arrange
        auto toybox = Runtime();
        RuntimeState& runtime = *toybox.state;
        Sandbox& sandbox = runtime.sandbox;
        initialize(runtime); // wires the VM backends
        const auto spawner = load_source(runtime.scripts, "spawner", R"(
function start(toy)
    local friend = tbx.sandbox.spawn("Friend")
    friend:sticker("summoned")
end
)");
        ASSERT_TRUE(spawner.has_value());
        sandbox.spawn("Summoner").with(Script {.source = *spawner});

        // Act
        update_scripts(runtime.scripts, sandbox, runtime.assets, runtime.events, 0.016f);

        // Assert
        const auto summoned = sandbox.find("Friend");
        ASSERT_TRUE(summoned.has_value());
        EXPECT_TRUE(Toy(*summoned).has_sticker("summoned"));
    }

    TEST(Scripts, FixedUpdateRunsAtFixedCadenceOnly)
    {
        // Arrange
        auto toybox = Runtime();
        RuntimeState& runtime = *toybox.state;
        Sandbox& sandbox = runtime.sandbox;
        initialize(runtime); // wires the VM backends
        const auto stepper = load_source(runtime.scripts, "stepper", R"(
function fixed_update(toy, delta_time)
    local position = toy.Transform.position
    toy.Transform.position = { x = position.x + 1.0, y = position.y, z = position.z }
end
)");
        ASSERT_TRUE(stepper.has_value());
        Toy toy = sandbox.spawn("Stepper").with(Script {.source = *stepper});

        // Act: variable updates do not run the fixed hook; fixed steps do.
        update_scripts(runtime.scripts, sandbox, runtime.assets, runtime.events, 0.016f);
        update_scripts(runtime.scripts, sandbox, runtime.assets, runtime.events, 0.016f);
        const float after_updates = toy.get_block<Transform>().position.x;
        fixed_update_scripts(runtime.scripts, 1.0f / 60.0f);
        fixed_update_scripts(runtime.scripts, 1.0f / 60.0f);
        fixed_update_scripts(runtime.scripts, 1.0f / 60.0f);

        // Assert
        EXPECT_EQ(after_updates, 0.0f);
        EXPECT_EQ(toy.get_block<Transform>().position.x, 3.0f);
    }

    TEST(Scripts, MissingBlockReadsAsNilAndAssignmentAttaches)
    {
        // Arrange
        auto toybox = Runtime();
        RuntimeState& runtime = *toybox.state;
        Sandbox& sandbox = runtime.sandbox;
        initialize(runtime); // wires the VM backends
        const auto builder = load_source(runtime.scripts, "builder", R"(
function start(toy)
    if toy.RigidBody == nil then
        toy:set_name("bare")
    end
    toy.RigidBody = { mass = 5.0, is_kinematic = true }
end
)");
        ASSERT_TRUE(builder.has_value());
        Toy toy = sandbox.spawn("Buildable").with(Script {.source = *builder});

        // Act
        update_scripts(runtime.scripts, sandbox, runtime.assets, runtime.events, 0.016f);

        // Assert: the nil read proved absence, the assignment attached and populated.
        EXPECT_EQ(toy.get_name(), "bare");
        ASSERT_TRUE(toy.has_block<RigidBody>());
        EXPECT_EQ(toy.get_block<RigidBody>().mass, 5.0f);
        EXPECT_TRUE(toy.get_block<RigidBody>().is_kinematic);
    }

    TEST(Scripts, TbxMathMirrorsTheEngineMathLibrary)
    {
        // Arrange
        auto toybox = Runtime();
        RuntimeState& runtime = *toybox.state;
        Sandbox& sandbox = runtime.sandbox;
        initialize(runtime); // wires the VM backends
        const auto mathy = load_source(runtime.scripts, "mathy", R"(
function start(toy)
    local moved = tbx.math.add({ x = 1.0, y = 2.0, z = 3.0 }, { x = 1.0, y = 0.0, z = 0.0 })
    local yawed = tbx.math.rotate(
        tbx.math.angle_axis(math.pi * 0.5, { x = 0.0, y = 1.0, z = 0.0 }),
        { x = 0.0, y = 0.0, z = -1.0 })
    toy.Transform.position = { x = moved.x + yawed.x, y = moved.y, z = moved.z }
end
)");
        ASSERT_TRUE(mathy.has_value());
        Toy toy = sandbox.spawn("Mathy").with(Script {.source = *mathy});

        // Act
        update_scripts(runtime.scripts, sandbox, runtime.assets, runtime.events, 0.016f);

        // Assert: (1+1) + rotate(-Z by 90° yaw).x = 2 + (-1) = 1.
        const Vec3 position = toy.get_block<Transform>().position;
        EXPECT_NEAR(position.x, 1.0f, 0.0001f);
        EXPECT_NEAR(position.y, 2.0f, 0.0001f);
        EXPECT_NEAR(position.z, 3.0f, 0.0001f);
    }

    TEST(Scripts, InputEnumsAreExposedToScripts)
    {
        // Arrange
        auto toybox = Runtime();
        RuntimeState& runtime = *toybox.state;
        Sandbox& sandbox = runtime.sandbox;
        initialize(runtime); // wires the VM backends
        const auto typed = load_source(runtime.scripts, "typed", R"(
function start(toy)
    -- tbx.Key/tbx.MouseButton are enum tables; prove they exist and are numbers.
    toy.Transform.position = {
        x = tbx.Key.W,
        y = tbx.Key.ESCAPE,
        z = tbx.MouseButton.LEFT,
    }
end
)");
        ASSERT_TRUE(typed.has_value());
        Toy toy = sandbox.spawn("Typist").with(Script {.source = *typed});

        // Act
        update_scripts(runtime.scripts, sandbox, runtime.assets, runtime.events, 0.016f);

        // Assert
        const Vec3 position = toy.get_block<Transform>().position;
        EXPECT_EQ(position.x, static_cast<float>(static_cast<int>(Key::W)));
        EXPECT_EQ(position.y, static_cast<float>(static_cast<int>(Key::ESCAPE)));
        EXPECT_EQ(position.z, static_cast<float>(static_cast<int>(MouseButton::LEFT)));
    }

    TEST(Scripts, DisabledToysDoNotRunScripts)
    {
        // Arrange
        auto toybox = Runtime();
        RuntimeState& runtime = *toybox.state;
        Sandbox& sandbox = runtime.sandbox;
        initialize(runtime); // wires the VM backends
        const auto mover = load_source(runtime.scripts, "mover", MOVER_SOURCE);
        ASSERT_TRUE(mover.has_value());
        Toy toy = sandbox.spawn("Grunt").with(Script {.source = *mover});
        toy.set_enabled(false);

        // Act
        update_scripts(runtime.scripts, sandbox, runtime.assets, runtime.events, 0.016f);

        // Assert: neither start nor update ran; re-enabling wakes it up.
        EXPECT_EQ(toy.get_name(), "Grunt");
        toy.set_enabled(true);
        update_scripts(runtime.scripts, sandbox, runtime.assets, runtime.events, 0.016f);
        EXPECT_EQ(toy.get_name(), "started");
    }

    TEST(Scripts, MissingUpdateFunctionIsHarmless)
    {
        // Arrange
        auto toybox = Runtime();
        RuntimeState& runtime = *toybox.state;
        Sandbox& sandbox = runtime.sandbox;
        initialize(runtime); // wires the VM backends
        const auto silent = load_source(runtime.scripts, "silent", "local nothing_defined = true");
        ASSERT_TRUE(silent.has_value());
        sandbox.spawn("Quiet").with(Script {.source = *silent});

        // Act / Assert: surviving both frames IS the behavior.
        update_scripts(runtime.scripts, sandbox, runtime.assets, runtime.events, 0.016f);
        update_scripts(runtime.scripts, sandbox, runtime.assets, runtime.events, 0.016f);
        SUCCEED();
    }
}
