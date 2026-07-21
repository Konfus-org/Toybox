#include "tbx/systems/connections/property_connection_system.h"
#include "tbx/types/assets/world.h"
#include "tbx/types/components/property_connections.h"
#include "tbx/types/components/transform.h"
#include <gtest/gtest.h>

namespace toybox_tests
{
    // A connection copies its source property onto its target each evaluation pass.
    TEST(PropertyConnectionTests, DrivesTargetPropertyFromSource)
    {
        auto world = tbx::World();
        auto source = world.create_entity("source");
        auto target = world.create_entity("target");
        source.add_component<tbx::Transform>().position = { 1.0F, 2.0F, 3.0F };
        target.add_component<tbx::Transform>();

        auto& connections = target.add_component<tbx::PropertyConnections>();
        connections.connections.push_back(tbx::PropertyConnection {
            .source_entity = source.get_id(),
            .source_component = "transform",
            .source_field = "position",
            .target_component = "transform",
            .target_field = "position",
        });

        tbx::evaluate_property_connections(world);

        const auto& driven = target.get_component<tbx::Transform>().position;
        EXPECT_FLOAT_EQ(driven.x, 1.0F);
        EXPECT_FLOAT_EQ(driven.y, 2.0F);
        EXPECT_FLOAT_EQ(driven.z, 3.0F);
    }

    // The source keeps driving: a later source change lands on the target the next pass.
    TEST(PropertyConnectionTests, ReappliesEachPass)
    {
        auto world = tbx::World();
        auto source = world.create_entity("source");
        auto target = world.create_entity("target");
        source.add_component<tbx::Transform>().position = { 1.0F, 0.0F, 0.0F };
        target.add_component<tbx::Transform>();

        auto& connections = target.add_component<tbx::PropertyConnections>();
        connections.connections.push_back(tbx::PropertyConnection {
            .source_entity = source.get_id(),
            .source_component = "transform",
            .source_field = "position",
            .target_component = "transform",
            .target_field = "position",
        });

        tbx::evaluate_property_connections(world);
        source.get_component<tbx::Transform>().position.x = 9.0F;
        tbx::evaluate_property_connections(world);

        EXPECT_FLOAT_EQ(target.get_component<tbx::Transform>().position.x, 9.0F);
    }

    // A connection whose source entity is gone is skipped — the target keeps its last value and the
    // pass never fails.
    TEST(PropertyConnectionTests, SkipsDeadSource)
    {
        auto world = tbx::World();
        auto target = world.create_entity("target");
        target.add_component<tbx::Transform>().position = { 5.0F, 0.0F, 0.0F };

        auto& connections = target.add_component<tbx::PropertyConnections>();
        connections.connections.push_back(tbx::PropertyConnection {
            .source_entity = tbx::Uuid(123456U),
            .source_component = "transform",
            .source_field = "position",
            .target_component = "transform",
            .target_field = "position",
        });

        tbx::evaluate_property_connections(world);

        EXPECT_FLOAT_EQ(target.get_component<tbx::Transform>().position.x, 5.0F);
    }
}
