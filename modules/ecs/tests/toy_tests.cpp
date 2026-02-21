#include "tbx/ecs/entity.h"

namespace tbx::tests::ecs
{
    TEST(ECSTests, CreatesEntityWithDescription)
    {
        EntityRegistry ecs = {};

        auto entity = Entity("Player", ecs);
        entity.set_tag("Hero");
        entity.set_layer("Gameplay");

        EXPECT_EQ(entity.get_name(), "Player");
        EXPECT_EQ(entity.get_tag(), "Hero");
        EXPECT_EQ(entity.get_layer(), "Gameplay");

        Uuid parent = Uuid(99U);
        entity.set_parent(parent);
        EXPECT_EQ(entity.get_parent(), parent);
    }
}
