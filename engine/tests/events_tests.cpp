#include "tbx/events/events.h"
#include <gtest/gtest.h>
#include <vector>

namespace tbx::tests
{
    TEST(Events, EmitDeliversOnDrain)
    {
        // Arrange
        events::purge();
        auto received = std::vector<int>();
        events::window_resized().subscribe(
            &received,
            [&received](const WindowResized& e) { received.push_back(e.width); });

        // Act
        events::window_resized().emit({.width = 800, .height = 600});
        events::drain();

        // Assert
        ASSERT_EQ(received.size(), 1u);
        EXPECT_EQ(received[0], 800);
    }

    TEST(Events, EmitDoesNotDeliverBeforeDrain)
    {
        // Arrange
        events::purge();
        auto received = 0;
        events::window_resized().subscribe(
            &received,
            [&received](const WindowResized&) { ++received; });

        // Act
        events::window_resized().emit({.width = 800, .height = 600});

        // Assert
        EXPECT_EQ(received, 0);
    }

    TEST(Events, EmitDuringDrainLandsInNextFrame)
    {
        // Arrange
        events::purge();
        auto deliveries = 0;
        events::window_resized().subscribe(
            &deliveries,
            [&](const WindowResized& e)
            {
                ++deliveries;
                // Re-emit once from inside dispatch; it must not run this drain.
                if (e.width == 1)
                    events::window_resized().emit({.width = 2, .height = 0});
            });

        // Act
        events::window_resized().emit({.width = 1, .height = 0});
        events::drain();
        const int after_first_drain = deliveries;
        events::drain();

        // Assert
        EXPECT_EQ(after_first_drain, 1);
        EXPECT_EQ(deliveries, 2);
    }

    TEST(Events, UnsubscribeOwnerRemovesAllOwnerHandlers)
    {
        // Arrange
        events::purge();
        auto owner_calls = 0;
        auto other_calls = 0;
        auto owner_tag = 1;
        auto other_tag = 2;
        events::key().subscribe(&owner_tag, [&owner_calls](const KeyEvent&) { ++owner_calls; });
        events::key().subscribe(&owner_tag, [&owner_calls](const KeyEvent&) { ++owner_calls; });
        events::key().subscribe(&other_tag, [&other_calls](const KeyEvent&) { ++other_calls; });

        // Act
        events::key().unsubscribe_owner(&owner_tag);
        events::key().emit({.key = Key::SPACE, .is_down = true, .is_repeat = false});
        events::drain();

        // Assert
        EXPECT_EQ(owner_calls, 0);
        EXPECT_EQ(other_calls, 1);
    }

    TEST(Events, UnsubscribeByTokenRemovesOnlyThatHandler)
    {
        // Arrange
        events::purge();
        auto first_calls = 0;
        auto second_calls = 0;
        auto tag = 0;
        const Token first = events::key().subscribe(&tag, [&](const KeyEvent&) { ++first_calls; });
        events::key().subscribe(&tag, [&](const KeyEvent&) { ++second_calls; });

        // Act
        events::key().unsubscribe(first);
        events::key().emit({.key = Key::A, .is_down = true, .is_repeat = false});
        events::drain();

        // Assert
        EXPECT_EQ(first_calls, 0);
        EXPECT_EQ(second_calls, 1);
    }

    TEST(Events, MixedSignalsDrainInEmissionOrder)
    {
        // Arrange
        events::purge();
        auto order = std::vector<int>();
        events::window_resized().subscribe(&order, [&](const WindowResized&) { order.push_back(1); });
        events::key().subscribe(&order, [&](const KeyEvent&) { order.push_back(2); });

        // Act
        events::window_resized().emit({.width = 1, .height = 1});
        events::key().emit({.key = Key::A, .is_down = true, .is_repeat = false});
        events::window_resized().emit({.width = 2, .height = 2});
        events::drain();

        // Assert
        ASSERT_EQ(order.size(), 3u);
        EXPECT_EQ(order[0], 1);
        EXPECT_EQ(order[1], 2);
        EXPECT_EQ(order[2], 1);
    }
}
