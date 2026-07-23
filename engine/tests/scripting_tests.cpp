#include "tbx/assets/assets.h"
#include "tbx/math/transform.h"
#include "tbx/physics/rigid_body.h"
#include "tbx/platform/keys.h"
#include "tbx/reflection/reflection.h"
#include "tbx/runtime.h"
#include "tbx/scripting/scripts.h"
#include "tbx/scripting/source.h"
#include "tbx/serialization/serializers.h"
#include "tbx/utils/hash.h"
#include "tbx/utils/result.h"
#include <any>
#include <gtest/gtest.h>

namespace tbx
{
    // A stable id for a named test script (mirrors how the asset system keys a source by path).
    static Uuid script_id(const std::string& name)
    {
        const uint64 hashed = hash(name);
        return Uuid {.hi = hashed, .lo = ~hashed};
    }

    // Registers a script from a string the way the asset pipeline does: seeds the ScriptSource
    // asset (so update_scripts can acquire it) and compiles it under the same id. Returns the
    // handle a Script block references, or the compile error.
    static Result<AssetHandle<ScriptSource>> given_script(
        internal::RuntimeState& runtime,
        const std::string& name,
        const std::string_view source)
    {
        const Uuid id = script_id(name);
        store_asset(
            runtime.assets,
            runtime.events,
            id,
            name + ".luau",
            std::any(ScriptSource {.text = std::string(source)}));
        if (const auto compiled = compile_script(runtime.scripts, id, name, source); !compiled)
            return std::unexpected(compiled.error());
        return ok(AssetHandle<ScriptSource>(id));
    }

    // Recompiles a named script (the hot-reload path) — same id as given_script.
    static Result<void> reload_test_script(
        internal::RuntimeState& runtime,
        const std::string& name,
        const std::string_view source)
    {
        return compile_script(runtime.scripts, script_id(name), name, source);
    }

    // Reflection + serialization must be ready before scripting (internal::initialize_scripting() asserts
    // it); the registries are process-global and self-guarding, so this is safe to call per test.
    static void boot(internal::RuntimeState& runtime)
    {
        initialize_reflection();
        register_builtin_serializers();
        internal::initialize_scripting(runtime); // wires the VM backends
    }

    static constexpr const char* MOVER_SOURCE = R"(
function start(toy)
    toy:rename("started")
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
        internal::RuntimeState& runtime = *toybox.state;
        Sandbox& sandbox = runtime.sandbox;
        boot(runtime);
        const auto mover = given_script(runtime, "mover", MOVER_SOURCE);
        ASSERT_TRUE(mover.has_value());
        Toy toy = sandbox.add("Grunt").with(Script {.source = *mover});

        // Act
        internal::update_scripts(runtime.scripts, sandbox, runtime.assets, runtime.events, 0.016f);
        internal::update_scripts(runtime.scripts, sandbox, runtime.assets, runtime.events, 0.016f);

        // Assert: start renamed once; update advanced position twice.
        EXPECT_EQ(toy.get_name(), "started");
        EXPECT_EQ(toy.add<Transform>().position.x, 2.0f);
    }

    TEST(Scripts, LoadRejectsBadSyntax)
    {
        // Arrange
        auto toybox = Runtime();
        internal::RuntimeState& runtime = *toybox.state;
        boot(runtime);

        // Act: compilation happens at registration, so bad syntax is rejected up front.
        const auto result = given_script(runtime, "broken", "this is not luau ((");

        // Assert
        EXPECT_FALSE(result.has_value());
    }

    TEST(Scripts, ReloadRestartsInstances)
    {
        // Arrange
        auto toybox = Runtime();
        internal::RuntimeState& runtime = *toybox.state;
        Sandbox& sandbox = runtime.sandbox;
        boot(runtime);
        const auto mover = given_script(runtime, "mover", MOVER_SOURCE);
        ASSERT_TRUE(mover.has_value());
        Toy toy = sandbox.add("Grunt").with(Script {.source = *mover});
        internal::update_scripts(runtime.scripts, sandbox, runtime.assets, runtime.events, 0.016f);
        toy.set_name("renamed-by-test");

        // Act: v2 renames differently on start; the instance must restart on the new code.
        const auto reloaded = reload_test_script(runtime, "mover", R"(
function start(toy)
    toy:rename("restarted")
end
)");
        ASSERT_TRUE(reloaded.has_value()) << reloaded.error();
        internal::update_scripts(runtime.scripts, sandbox, runtime.assets, runtime.events, 0.016f);

        // Assert
        EXPECT_EQ(toy.get_name(), "restarted");
    }

    TEST(Scripts, ReloadWithBadSyntaxKeepsOldBehavior)
    {
        // Arrange
        auto toybox = Runtime();
        internal::RuntimeState& runtime = *toybox.state;
        Sandbox& sandbox = runtime.sandbox;
        boot(runtime);
        const auto mover = given_script(runtime, "mover", MOVER_SOURCE);
        ASSERT_TRUE(mover.has_value());
        Toy toy = sandbox.add("Grunt").with(Script {.source = *mover});
        internal::update_scripts(runtime.scripts, sandbox, runtime.assets, runtime.events, 0.016f);

        // Act
        const auto reloaded = reload_test_script(runtime, "mover", "broken ((");
        internal::update_scripts(runtime.scripts, sandbox, runtime.assets, runtime.events, 0.016f);

        // Assert: reload failed (old bytecode kept), so the original script keeps moving the toy.
        EXPECT_FALSE(reloaded.has_value());
        EXPECT_EQ(toy.add<Transform>().position.x, 2.0f);
    }

    TEST(Scripts, ScriptCanSpawnAndStickerThroughTbxApi)
    {
        // Arrange
        auto toybox = Runtime();
        internal::RuntimeState& runtime = *toybox.state;
        Sandbox& sandbox = runtime.sandbox;
        boot(runtime);
        const auto spawner = given_script(runtime, "spawner", R"(
function start(toy)
    local friend = tbx.sandbox:add("Friend")
    friend:add("summoned")
end
)");
        ASSERT_TRUE(spawner.has_value());
        sandbox.add("Summoner").with(Script {.source = *spawner});

        // Act
        internal::update_scripts(runtime.scripts, sandbox, runtime.assets, runtime.events, 0.016f);

        // Assert
        const auto summoned = sandbox.find("Friend");
        ASSERT_TRUE(summoned.has_value());
        EXPECT_TRUE(Toy(*summoned).has("summoned"));
    }

    TEST(Scripts, FixedUpdateRunsAtFixedCadenceOnly)
    {
        // Arrange
        auto toybox = Runtime();
        internal::RuntimeState& runtime = *toybox.state;
        Sandbox& sandbox = runtime.sandbox;
        boot(runtime);
        const auto stepper = given_script(runtime, "stepper", R"(
function fixedUpdate(toy, delta_time)
    local position = toy.Transform.position
    toy.Transform.position = { x = position.x + 1.0, y = position.y, z = position.z }
end
)");
        ASSERT_TRUE(stepper.has_value());
        Toy toy = sandbox.add("Stepper").with(Script {.source = *stepper});

        // Act: variable updates do not run the fixed hook; fixed steps do.
        internal::update_scripts(runtime.scripts, sandbox, runtime.assets, runtime.events, 0.016f);
        internal::update_scripts(runtime.scripts, sandbox, runtime.assets, runtime.events, 0.016f);
        const float after_updates = toy.add<Transform>().position.x;
        internal::fixed_update_scripts(runtime.scripts, sandbox, 1.0f / 60.0f);
        internal::fixed_update_scripts(runtime.scripts, sandbox, 1.0f / 60.0f);
        internal::fixed_update_scripts(runtime.scripts, sandbox, 1.0f / 60.0f);

        // Assert
        EXPECT_EQ(after_updates, 0.0f);
        EXPECT_EQ(toy.add<Transform>().position.x, 3.0f);
    }

    TEST(Scripts, MissingBlockReadsAsNilAndAssignmentAttaches)
    {
        // Arrange
        auto toybox = Runtime();
        internal::RuntimeState& runtime = *toybox.state;
        Sandbox& sandbox = runtime.sandbox;
        boot(runtime);
        const auto builder = given_script(runtime, "builder", R"(
function start(toy)
    if toy.RigidBody == nil then
        toy:rename("bare")
    end
    toy:add(tbx.blocks.RigidBody, { mass = 5.0, is_kinematic = true })
end
)");
        ASSERT_TRUE(builder.has_value());
        Toy toy = sandbox.add("Buildable").with(Script {.source = *builder});

        // Act
        internal::update_scripts(runtime.scripts, sandbox, runtime.assets, runtime.events, 0.016f);

        // Assert: the nil read proved absence, the assignment attached and populated.
        EXPECT_EQ(toy.get_name(), "bare");
        ASSERT_TRUE(toy.has<RigidBody>());
        EXPECT_EQ(toy.add<RigidBody>().mass, 5.0f);
        EXPECT_TRUE(toy.add<RigidBody>().is_kinematic);
    }

    TEST(Scripts, CustomBlockAutoMintsAddsReadsAndRemoves)
    {
        // Arrange: naming a non-built-in block (tbx.blocks.Ammo) mints a script-defined block on
        // first access — no register call. It is a dynamic field bag, runtime-only. The script
        // signals each step's outcome back to C++ with stickers.
        auto toybox = Runtime();
        internal::RuntimeState& runtime = *toybox.state;
        Sandbox& sandbox = runtime.sandbox;
        boot(runtime);
        const auto gunner = given_script(runtime, "gunner", R"(
function start(toy)
    local Ammo = tbx.blocks.Ammo
    if not toy:has(Ammo) then toy:add("absent_before") end
    local ammo = toy:add(Ammo, { count = 2 })
    if toy:has(Ammo) then toy:add("present_after_add") end
    if ammo.count == 2 then toy:add("fields_populated") end
    ammo.count = ammo.count - 1
    if toy:get(Ammo).count == 1 then toy:add("live_table_mutated") end
    toy:remove(Ammo)
    if not toy:has(Ammo) then toy:add("removed") end
end
)");
        ASSERT_TRUE(gunner.has_value());
        Toy toy = sandbox.add("Gunner").with(Script {.source = *gunner});

        // Act
        internal::update_scripts(runtime.scripts, sandbox, runtime.assets, runtime.events, 0.016f);

        // Assert: every step ran as expected.
        EXPECT_TRUE(toy.has("absent_before"));
        EXPECT_TRUE(toy.has("present_after_add"));
        EXPECT_TRUE(toy.has("fields_populated"));
        EXPECT_TRUE(toy.has("live_table_mutated"));
        EXPECT_TRUE(toy.has("removed"));
    }

    TEST(Scripts, TbxMathMirrorsTheEngineMathLibrary)
    {
        // Arrange
        auto toybox = Runtime();
        internal::RuntimeState& runtime = *toybox.state;
        Sandbox& sandbox = runtime.sandbox;
        boot(runtime);
        const auto mathy = given_script(runtime, "mathy", R"(
function start(toy)
    local moved = tbx.math.add({ x = 1.0, y = 2.0, z = 3.0 }, { x = 1.0, y = 0.0, z = 0.0 })
    local yawed = tbx.math.rotate(
        tbx.math.angleAxis(math.pi * 0.5, { x = 0.0, y = 1.0, z = 0.0 }),
        { x = 0.0, y = 0.0, z = -1.0 })
    toy.Transform.position = { x = moved.x + yawed.x, y = moved.y, z = moved.z }
end
)");
        ASSERT_TRUE(mathy.has_value());
        Toy toy = sandbox.add("Mathy").with(Script {.source = *mathy});

        // Act
        internal::update_scripts(runtime.scripts, sandbox, runtime.assets, runtime.events, 0.016f);

        // Assert: (1+1) + rotate(-Z by 90° yaw).x = 2 + (-1) = 1.
        const Vec3 position = toy.add<Transform>().position;
        EXPECT_NEAR(position.x, 1.0f, 0.0001f);
        EXPECT_NEAR(position.y, 2.0f, 0.0001f);
        EXPECT_NEAR(position.z, 3.0f, 0.0001f);
    }

    TEST(Scripts, InputEnumsAreExposedToScripts)
    {
        // Arrange
        auto toybox = Runtime();
        internal::RuntimeState& runtime = *toybox.state;
        Sandbox& sandbox = runtime.sandbox;
        boot(runtime);
        const auto typed = given_script(runtime, "typed", R"(
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
        Toy toy = sandbox.add("Typist").with(Script {.source = *typed});

        // Act
        internal::update_scripts(runtime.scripts, sandbox, runtime.assets, runtime.events, 0.016f);

        // Assert. Keys are exposed 1:1; mouse/gamepad enums are offset into their own value
        // range (the luau input dispatch), so tbx.MouseButton.LEFT reads 1000 + LEFT.
        const Vec3 position = toy.add<Transform>().position;
        EXPECT_EQ(position.x, static_cast<float>(static_cast<int>(Key::W)));
        EXPECT_EQ(position.y, static_cast<float>(static_cast<int>(Key::ESCAPE)));
        EXPECT_EQ(position.z, static_cast<float>(1000 + static_cast<int>(MouseButton::LEFT)));
    }

    TEST(Scripts, DisabledToysDoNotRunScripts)
    {
        // Arrange
        auto toybox = Runtime();
        internal::RuntimeState& runtime = *toybox.state;
        Sandbox& sandbox = runtime.sandbox;
        boot(runtime);
        const auto mover = given_script(runtime, "mover", MOVER_SOURCE);
        ASSERT_TRUE(mover.has_value());
        Toy toy = sandbox.add("Grunt").with(Script {.source = *mover});
        toy.set_enabled(false);

        // Act
        internal::update_scripts(runtime.scripts, sandbox, runtime.assets, runtime.events, 0.016f);

        // Assert: neither start nor update ran; re-enabling wakes it up.
        EXPECT_EQ(toy.get_name(), "Grunt");
        toy.set_enabled(true);
        internal::update_scripts(runtime.scripts, sandbox, runtime.assets, runtime.events, 0.016f);
        EXPECT_EQ(toy.get_name(), "started");
    }

    TEST(Scripts, MissingUpdateFunctionIsHarmless)
    {
        // Arrange
        auto toybox = Runtime();
        internal::RuntimeState& runtime = *toybox.state;
        Sandbox& sandbox = runtime.sandbox;
        boot(runtime);
        const auto silent = given_script(runtime, "silent", "local nothing_defined = true");
        ASSERT_TRUE(silent.has_value());
        sandbox.add("Quiet").with(Script {.source = *silent});

        // Act / Assert: surviving both frames IS the behavior.
        internal::update_scripts(runtime.scripts, sandbox, runtime.assets, runtime.events, 0.016f);
        internal::update_scripts(runtime.scripts, sandbox, runtime.assets, runtime.events, 0.016f);
        SUCCEED();
    }
}
