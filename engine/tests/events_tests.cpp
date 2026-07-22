#include "tbx/events/events.h"
#include <gtest/gtest.h>
#include <vector>

namespace tbx::tests
{
    TEST(Events, EmitDeliversOnDrain)
    {
        // Arrange
        auto events = events::EventsState();
        auto received = std::vector<int>();
        events.window_resized.subscribe(
            &received,
            [&received](const events::WindowResized& e) { received.push_back(e.width); });

        // Act
        events.window_resized.emit({.width = 800, .height = 600});
        events::update(events);

        // Assert
        ASSERT_EQ(received.size(), 1u);
        EXPECT_EQ(received[0], 800);
    }

    TEST(Events, EmitDoesNotDeliverBeforeDrain)
    {
        // Arrange
        auto events = events::EventsState();
        auto received = 0;
        events.window_resized.subscribe(
            &received,
            [&received](const events::WindowResized&) { ++received; });

        // Act
        events.window_resized.emit({.width = 800, .height = 600});

        // Assert
        EXPECT_EQ(received, 0);
    }

    TEST(Events, EmitDuringDrainLandsInNextFrame)
    {
        // Arrange
        auto events = events::EventsState();
        auto deliveries = 0;
        events.window_resized.subscribe(
            &deliveries,
            [&](const events::WindowResized& e)
            {
                ++deliveries;
                // Re-emit once from inside dispatch; it must not run this drain.
                if (e.width == 1)
                    events.window_resized.emit({.width = 2, .height = 0});
            });

        // Act
        events.window_resized.emit({.width = 1, .height = 0});
        events::update(events);
        const int after_first_drain = deliveries;
        events::update(events);

        // Assert
        EXPECT_EQ(after_first_drain, 1);
        EXPECT_EQ(deliveries, 2);
    }

    TEST(Events, UnsubscribeOwnerRemovesAllOwnerHandlers)
    {
        // Arrange
        auto events = events::EventsState();
        auto owner_calls = 0;
        auto other_calls = 0;
        auto owner_tag = 1;
        auto other_tag = 2;
        events.key.subscribe(&owner_tag, [&owner_calls](const events::KeyEvent&) { ++owner_calls; });
        events.key.subscribe(&owner_tag, [&owner_calls](const events::KeyEvent&) { ++owner_calls; });
        events.key.subscribe(&other_tag, [&other_calls](const events::KeyEvent&) { ++other_calls; });

        // Act
        events.key.unsubscribe_owner(&owner_tag);
        events.key.emit({.key = input::Key::SPACE, .is_down = true, .is_repeat = false});
        events::update(events);

        // Assert
        EXPECT_EQ(owner_calls, 0);
        EXPECT_EQ(other_calls, 1);
    }

    TEST(Events, UnsubscribeByTokenRemovesOnlyThatHandler)
    {
        // Arrange
        auto events = events::EventsState();
        auto first_calls = 0;
        auto second_calls = 0;
        auto tag = 0;
        const events::Token first = events.key.subscribe(&tag, [&](const events::KeyEvent&) { ++first_calls; });
        events.key.subscribe(&tag, [&](const events::KeyEvent&) { ++second_calls; });

        // Act
        events.key.unsubscribe(first);
        events.key.emit({.key = input::Key::A, .is_down = true, .is_repeat = false});
        events::update(events);

        // Assert
        EXPECT_EQ(first_calls, 0);
        EXPECT_EQ(second_calls, 1);
    }

    TEST(Events, MixedSignalsDrainInEmissionOrder)
    {
        // Arrange
        auto events = events::EventsState();
        auto order = std::vector<int>();
        events.window_resized.subscribe(&order, [&](const events::WindowResized&) { order.push_back(1); });
        events.key.subscribe(&order, [&](const events::KeyEvent&) { order.push_back(2); });

        // Act
        events.window_resized.emit({.width = 1, .height = 1});
        events.key.emit({.key = input::Key::A, .is_down = true, .is_repeat = false});
        events.window_resized.emit({.width = 2, .height = 2});
        events::update(events);

        // Assert
        ASSERT_EQ(order.size(), 3u);
        EXPECT_EQ(order[0], 1);
        EXPECT_EQ(order[1], 2);
        EXPECT_EQ(order[2], 1);
    }
}
