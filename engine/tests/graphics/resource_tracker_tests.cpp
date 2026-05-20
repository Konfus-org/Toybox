#include "tbx/systems/graphics/resource_tracker.h"
#include <gtest/gtest.h>

namespace tbx::tests::graphics
{
    TEST(RenderingResourceTrackerTests, Track_AddsResourceAndResetsAge)
    {
        // Arrange
        auto tracker = RenderingResourceTracker();

        // Act
        tracker.track(42U);
        tracker.update(DeltaTime {.seconds = 1.5, .milliseconds = 1500.0});
        tracker.track(42U);

        // Assert
        ASSERT_TRUE(tracker.is_tracked(42U));
        EXPECT_EQ(tracker.get_tracked_resources().size(), 1U);
        EXPECT_FLOAT_EQ(tracker.get_time_alive(42U), 0.0F);
    }

    TEST(RenderingResourceTrackerTests, Update_AdvancesTrackedResourceAge)
    {
        // Arrange
        auto tracker = RenderingResourceTracker();
        tracker.track(7U);

        // Act
        tracker.update(DeltaTime {.seconds = 0.25, .milliseconds = 250.0});

        // Assert
        EXPECT_FLOAT_EQ(tracker.get_time_alive(7U), 0.25F);
    }

    TEST(RenderingResourceTrackerTests, Untrack_RemovesResource)
    {
        // Arrange
        auto tracker = RenderingResourceTracker();
        tracker.track(9U);

        // Act
        tracker.untrack(9U);

        // Assert
        EXPECT_FALSE(tracker.is_tracked(9U));
        EXPECT_TRUE(tracker.get_tracked_resources().empty());
    }
}
