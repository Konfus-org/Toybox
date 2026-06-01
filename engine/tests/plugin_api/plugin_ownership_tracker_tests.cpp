#include "tbx/systems/plugin_api/plugin_ownership_tracker.h"
#include "tbx/systems/plugin_api/plugin_ownership.h"
#include "tbx/systems/plugin_api/service_provider.h"
#include <algorithm>

namespace tbx::tests::plugin_api
{
    struct DummyTrackedService
    {
        virtual ~DummyTrackedService() = default;
    };

    struct DummyTrackedServiceImpl final : DummyTrackedService
    {
    };

    TEST(PluginOwnershipTrackerTests, SnapshotAndClearIsDeterministicAndIdempotent)
    {
        // Arrange
        auto tracker = PluginOwnershipTracker {};
        const auto plugin_id = Uuid(42U);
        tracker.track_entity(plugin_id, Uuid(4U));
        tracker.track_entity(plugin_id, Uuid(2U));
        tracker.track_entity(plugin_id, Uuid(2U));
        tracker.track_serializable_registration(plugin_id, "b_name");
        tracker.track_serializable_registration(plugin_id, "a_name");
        tracker.track_serializable_registration(plugin_id, "a_name");
        tracker.track_asset_type_registration(plugin_id, std::type_index(typeid(float)));
        tracker.track_asset_type_registration(plugin_id, std::type_index(typeid(int)));
        tracker.track_asset_type_registration(plugin_id, std::type_index(typeid(int)));

        // Act
        const auto snapshot = tracker.snapshot_and_clear(plugin_id);
        const auto second_snapshot = tracker.snapshot_and_clear(plugin_id);

        // Assert
        ASSERT_EQ(snapshot.entity_ids.size(), 2U);
        EXPECT_EQ(snapshot.entity_ids[0], Uuid(2U));
        EXPECT_EQ(snapshot.entity_ids[1], Uuid(4U));
        ASSERT_EQ(snapshot.serializable_type_names.size(), 2U);
        EXPECT_EQ(snapshot.serializable_type_names[0], "a_name");
        EXPECT_EQ(snapshot.serializable_type_names[1], "b_name");
        ASSERT_EQ(snapshot.asset_types.size(), 2U);
        EXPECT_TRUE(
            std::ranges::is_sorted(
                snapshot.asset_types,
                [](std::type_index left, std::type_index right)
                {
                    return std::string(left.name()) < std::string(right.name());
                }));
        EXPECT_TRUE(second_snapshot.entity_ids.empty());
        EXPECT_TRUE(second_snapshot.serializable_type_names.empty());
        EXPECT_TRUE(second_snapshot.asset_types.empty());
    }

    TEST(PluginOwnershipTrackerTests, ServiceProviderRegistrationTracksActivePluginServiceType)
    {
        // Arrange
        auto tracker = std::make_shared<PluginOwnershipTracker>();
        bind_plugin_ownership_tracker(tracker);
        auto service_provider = ServiceProvider {};
        const auto plugin_id = Uuid(99U);

        // Act
        {
            auto plugin_scope = ScopedPluginContext(plugin_id);
            service_provider.register_service<DummyTrackedService>(
                std::make_shared<DummyTrackedServiceImpl>());
        }
        const auto snapshot = tracker->snapshot_and_clear(plugin_id);
        bind_plugin_ownership_tracker({});

        // Assert
        ASSERT_EQ(snapshot.service_types.size(), 1U);
        EXPECT_EQ(snapshot.service_types.front(), std::type_index(typeid(DummyTrackedService)));
    }
}
