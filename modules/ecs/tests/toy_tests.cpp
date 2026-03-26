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

    // Validates registry-level entity existence checks by id.
    TEST(ECSTests, HasById_TracksEntityLifetime)
    {
        // Arrange
        EntityRegistry ecs = {};
        const auto entity = Entity("LifetimeProbe", ecs);
        const auto entity_id = entity.get_id();

        // Act
        const bool has_before_destroy = ecs.has(entity_id);
        auto entity_to_destroy = ecs.get(entity_id);
        entity_to_destroy.destroy();
        const bool has_after_destroy = ecs.has(entity_id);
        const bool has_invalid_id = ecs.has(Uuid());

        // Assert
        EXPECT_TRUE(has_before_destroy);
        EXPECT_FALSE(has_after_destroy);
        EXPECT_FALSE(has_invalid_id);
    }
}
