#include "tbx/ecs/entities.h"

namespace tbx::tests::ecs
{
    TEST(ECSTests, CreatesEntityWithDescription)
    {
        ECS ecs = {};

        const auto entity = ecs.create_entity("Player", "Hero", "Gameplay");
        const auto& description = entity.get_description();

        EXPECT_EQ(description.name, "Player");
        EXPECT_EQ(description.tag, "Hero");
        EXPECT_EQ(description.layer, "Gameplay");
    }
}
