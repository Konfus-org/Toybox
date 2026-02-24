#include "tbx/ecs/entity.h"

namespace tbx::tests::ecs
{
    // Validates parent metadata roundtrip via id-based accessors.
    TEST(ECSTests, CreatesEntityWithDescription)
    {
        // Arrange
        EntityRegistry ecs = {};

        // Act
        auto entity = Entity("Player", ecs);
        entity.set_tag("Hero");
        entity.set_layer("Gameplay");

        // Assert
        EXPECT_EQ(entity.get_name(), "Player");
        EXPECT_EQ(entity.get_tag(), "Hero");
        EXPECT_EQ(entity.get_layer(), "Gameplay");

        Uuid parent = Uuid(99U);
        entity.set_parent(parent);
        EXPECT_EQ(entity.get_parent(), parent);
    }

    // Validates resolving a parent entity handle from a child entity.
    TEST(ECSTests, TryGetParentEntity_ReturnsParentEntity)
    {
        // Arrange
        EntityRegistry ecs = {};
        auto parent = Entity("PlayerRoot", ecs);
        auto child = Entity("PlayerVisual", parent.get_id(), ecs);

        // Act
        auto resolved_parent = Entity {};
        const bool has_parent = child.try_get_parent_entity(resolved_parent);

        // Assert
        EXPECT_TRUE(has_parent);
        EXPECT_EQ(resolved_parent.get_id(), parent.get_id());
        EXPECT_EQ(resolved_parent.get_name(), "PlayerRoot");
    }
}
