#include "tbx/scripting/scripts.h"
#include <gtest/gtest.h>

namespace tbx::tests
{
    static constexpr const char* MOVER_SOURCE = R"(
local script = {}
function script.start(toy)
    toy:set_name("started")
end
function script.update(toy, delta_time)
    local transform = toy:get("Transform")
    local position = transform.position
    transform.position = { x = position.x + 1.0, y = position.y, z = position.z }
end
return script
)";

    TEST(Scripts, StartRunsOnceAndUpdateMovesToy)
    {
        // Arrange
        auto jobs = Jobs();
        auto sandbox = Sandbox(jobs);
        auto events = Events();
        auto scripts = Scripts(sandbox, events);
        ASSERT_TRUE(scripts.load_source("mover", MOVER_SOURCE).has_value());
        Toy toy = sandbox.spawn("Grunt").with(Script {.source = "mover"});

        // Act
        scripts.update(0.016f);
        scripts.update(0.016f);

        // Assert: start renamed once; update advanced position twice.
        EXPECT_EQ(toy.get_name(), "started");
        EXPECT_EQ(toy.get_block<Transform>().position.x, 2.0f);
    }

    TEST(Scripts, LoadRejectsBadSyntax)
    {
        // Arrange
        auto jobs = Jobs();
        auto sandbox = Sandbox(jobs);
        auto events = Events();
        auto scripts = Scripts(sandbox, events);

        // Act
        const auto result = scripts.load_source("broken", "this is not luau ((");

        // Assert
        EXPECT_FALSE(result.has_value());
    }

    TEST(Scripts, ReloadRestartsInstancesAndFiresEvent)
    {
        // Arrange
        auto jobs = Jobs();
        auto sandbox = Sandbox(jobs);
        auto events = Events();
        auto scripts = Scripts(sandbox, events);
        auto reload_count = 0;
        events.script_reloaded.subscribe(
            &reload_count,
            [&reload_count](const ScriptReloaded&) { ++reload_count; });
        ASSERT_TRUE(scripts.load_source("mover", MOVER_SOURCE).has_value());
        Toy toy = sandbox.spawn("Grunt").with(Script {.source = "mover"});
        scripts.update(0.016f);
        toy.set_name("renamed-by-test");

        // Act: v2 renames differently on start; instance must restart.
        const auto reloaded = scripts.reload_source("mover", R"(
local script = {}
function script.start(toy)
    toy:set_name("restarted")
end
return script
)");
        scripts.update(0.016f);
        events.drain();

        // Assert
        ASSERT_TRUE(reloaded.has_value()) << reloaded.error();
        EXPECT_EQ(toy.get_name(), "restarted");
        EXPECT_EQ(reload_count, 1);
    }

    TEST(Scripts, ReloadWithBadSyntaxKeepsOldBehavior)
    {
        // Arrange
        auto jobs = Jobs();
        auto sandbox = Sandbox(jobs);
        auto events = Events();
        auto scripts = Scripts(sandbox, events);
        ASSERT_TRUE(scripts.load_source("mover", MOVER_SOURCE).has_value());
        Toy toy = sandbox.spawn("Grunt").with(Script {.source = "mover"});
        scripts.update(0.016f);

        // Act
        const auto reloaded = scripts.reload_source("mover", "broken ((");
        scripts.update(0.016f);

        // Assert: reload failed, old script keeps running.
        EXPECT_FALSE(reloaded.has_value());
        EXPECT_EQ(toy.get_block<Transform>().position.x, 2.0f);
    }

    TEST(Scripts, ScriptCanSpawnAndStickerThroughTbxApi)
    {
        // Arrange
        auto jobs = Jobs();
        auto sandbox = Sandbox(jobs);
        auto events = Events();
        auto scripts = Scripts(sandbox, events);
        ASSERT_TRUE(scripts
                        .load_source("spawner", R"(
local script = {}
function script.start(toy)
    local friend = tbx.sandbox.spawn("Friend")
    friend:sticker("summoned")
end
return script
)")
                        .has_value());
        sandbox.spawn("Summoner").with(Script {.source = "spawner"});

        // Act
        scripts.update(0.016f);

        // Assert
        const auto summoned = sandbox.find_toy("Friend");
        ASSERT_TRUE(summoned.has_value());
        EXPECT_TRUE(Toy(*summoned).has_sticker("summoned"));
    }

    TEST(Scripts, FixedUpdateRunsAtFixedCadenceOnly)
    {
        // Arrange
        auto jobs = Jobs();
        auto sandbox = Sandbox(jobs);
        auto events = Events();
        auto scripts = Scripts(sandbox, events);
        ASSERT_TRUE(scripts
                        .load_source("stepper", R"(
local script = {}
function script.fixed_update(toy, delta_time)
    local transform = toy:get("Transform")
    local position = transform.position
    transform.position = { x = position.x + 1.0, y = position.y, z = position.z }
end
return script
)")
                        .has_value());
        Toy toy = sandbox.spawn("Stepper").with(Script {.source = "stepper"});

        // Act: variable updates do not run the fixed hook; fixed steps do.
        scripts.update(0.016f);
        scripts.update(0.016f);
        const float after_updates = toy.get_block<Transform>().position.x;
        scripts.fixed_update(1.0f / 60.0f);
        scripts.fixed_update(1.0f / 60.0f);
        scripts.fixed_update(1.0f / 60.0f);

        // Assert
        EXPECT_EQ(after_updates, 0.0f);
        EXPECT_EQ(toy.get_block<Transform>().position.x, 3.0f);
    }

    TEST(Scripts, MissingUpdateFunctionIsHarmless)
    {
        // Arrange
        auto jobs = Jobs();
        auto sandbox = Sandbox(jobs);
        auto events = Events();
        auto scripts = Scripts(sandbox, events);
        ASSERT_TRUE(scripts.load_source("silent", "return {}").has_value());
        sandbox.spawn("Quiet").with(Script {.source = "silent"});

        // Act / Assert: surviving both frames IS the behavior.
        scripts.update(0.016f);
        scripts.update(0.016f);
        SUCCEED();
    }
}
