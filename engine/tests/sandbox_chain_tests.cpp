#include "tbx/ecs/sandbox.h"
#include "tbx/ecs/toy.h"
#include <gtest/gtest.h>

// The fluent world/toy verbs at the container level (Toy::spawn siblings, Sandbox::open modes,
// close). The free open(root)/close()/spawn() over the running runtime forward straight to these,
// and are exercised by the sample; here we test the logic that actually does the work.
namespace tbx
{
    TEST(SandboxChain, ToySpawnCreatesSiblingsInTheSameContainer)
    {
        // Arrange
        auto sandbox = Sandbox();

        // Act: a spawn chain keeps adding to the world (open(root).spawn("A").spawn("B") in miniature).
        Toy a = sandbox.spawn("A");
        Toy b = a.spawn("B");

        // Assert
        EXPECT_TRUE(a.is_alive());
        EXPECT_TRUE(b.is_alive());
        EXPECT_EQ(sandbox.get_toy_count(), size(2));
        EXPECT_FALSE(b.get_parent().has_value()); // sibling of A shares A's (null) parent
    }

    TEST(SandboxChain, ToySpawnInheritsTheParentOfItsOrigin)
    {
        // Arrange
        auto sandbox = Sandbox();
        Toy root = sandbox.spawn("Root");
        Toy child = sandbox.spawn("Child").set_parent(root);

        // Act: a sibling of a parented toy lands under the same parent, not under the toy itself.
        Toy sibling = child.spawn("Sibling");

        // Assert
        ASSERT_TRUE(sibling.get_parent().has_value());
        EXPECT_EQ(sibling.get_parent()->get_uuid(), root.get_uuid());
    }

    TEST(SandboxChain, OpenReplaceClearsTheWorldAndDefersTheRoot)
    {
        // Arrange
        auto sandbox = Sandbox();
        sandbox.spawn("Old");

        // Act
        Sandbox& returned = sandbox.open(AssetHandle<Kit>("root.kit"), OpenMode::REPLACE);

        // Assert
        EXPECT_EQ(&returned, &sandbox);              // fluent: returns the world
        EXPECT_EQ(sandbox.get_toy_count(), size(0)); // the old world is gone
        EXPECT_TRUE(sandbox.pending_root.has_value()); // the fresh root resolves on the next tick
    }

    TEST(SandboxChain, OpenAdditiveWithoutWiredAssetsFailsGracefully)
    {
        // Arrange: a sandbox with no asset system wired (the runtime wires it at boot).
        auto sandbox = Sandbox();

        // Act
        Sandbox& returned = sandbox.open(AssetHandle<Kit>("root.kit"), OpenMode::ADDITIVE);

        // Assert: still fluent, no crash, nothing added, and additive never defers.
        EXPECT_EQ(&returned, &sandbox);
        EXPECT_EQ(sandbox.get_toy_count(), size(0));
        EXPECT_FALSE(sandbox.pending_root.has_value());
    }

    TEST(SandboxChain, CloseEmptiesTheWorldAndClearsPendingOpens)
    {
        // Arrange
        auto sandbox = Sandbox();
        sandbox.spawn("A");
        sandbox.open(AssetHandle<Kit>("root.kit"), OpenMode::REPLACE); // clears + sets pending_root
        sandbox.spawn("B");

        // Act
        close(sandbox);

        // Assert
        EXPECT_EQ(sandbox.get_toy_count(), size(0));
        EXPECT_FALSE(sandbox.pending_root.has_value());
    }

    TEST(SandboxChain, CloseOnAnEmptyWorldIsANoOp)
    {
        // Arrange
        auto sandbox = Sandbox();

        // Act / Assert: closing an already-empty world does nothing and does not crash.
        close(sandbox);
        EXPECT_EQ(sandbox.get_toy_count(), size(0));
    }
}
