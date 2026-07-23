#include "tbx/events/events.h"
#include <gtest/gtest.h>
#include <vector>

namespace tbx
{
    TEST(Events, EmitDeliversOnDrain)
    {
        // Arrange
        auto events = EventsState();
        auto received = std::vector<int>();
        events.signal<WindowResized>().subscribe(
            &received,
            [&received](const WindowResized& e) { received.push_back(e.width); });

        // Act
        events.signal<WindowResized>().emit({.width = 800, .height = 600});
        internal::update_events(events);

        // Assert
        ASSERT_EQ(received.size(), 1u);
        EXPECT_EQ(received[0], 800);
    }

    TEST(Events, EmitDoesNotDeliverBeforeDrain)
    {
        // Arrange
        auto events = EventsState();
        auto received = 0;
        events.signal<WindowResized>().subscribe(
            &received,
            [&received](const WindowResized&) { ++received; });

        // Act
        events.signal<WindowResized>().emit({.width = 800, .height = 600});

        // Assert
        EXPECT_EQ(received, 0);
    }

    TEST(Events, EmitDuringDrainLandsInNextFrame)
    {
        // Arrange
        auto events = EventsState();
        auto deliveries = 0;
        events.signal<WindowResized>().subscribe(
            &deliveries,
            [&](const WindowResized& e)
            {
                ++deliveries;
                // Re-emit once from inside dispatch; it must not run this drain.
                if (e.width == 1)
                    events.signal<WindowResized>().emit({.width = 2, .height = 0});
            });

        // Act
        events.signal<WindowResized>().emit({.width = 1, .height = 0});
        internal::update_events(events);
        const int after_first_drain = deliveries;
        internal::update_events(events);

        // Assert
        EXPECT_EQ(after_first_drain, 1);
        EXPECT_EQ(deliveries, 2);
    }

    TEST(Events, UnsubscribeOwnerRemovesAllOwnerHandlers)
    {
        // Arrange
        auto events = EventsState();
        auto owner_calls = 0;
        auto other_calls = 0;
        auto owner_tag = 1;
        auto other_tag = 2;
        events.signal<InputEvent>().subscribe(&owner_tag, [&owner_calls](const InputEvent&) { ++owner_calls; });
        events.signal<InputEvent>().subscribe(&owner_tag, [&owner_calls](const InputEvent&) { ++owner_calls; });
        events.signal<InputEvent>().subscribe(&other_tag, [&other_calls](const InputEvent&) { ++other_calls; });

        // Act
        events.signal<InputEvent>().unsubscribe_owner(&owner_tag);
        events.signal<InputEvent>().emit({.key = Key::SPACE, .is_down = true, .is_repeat = false});
        internal::update_events(events);

        // Assert
        EXPECT_EQ(owner_calls, 0);
        EXPECT_EQ(other_calls, 1);
    }

    TEST(Events, UnsubscribeByTokenRemovesOnlyThatHandler)
    {
        // Arrange
        auto events = EventsState();
        auto first_calls = 0;
        auto second_calls = 0;
        auto tag = 0;
        const Token first = events.signal<InputEvent>().subscribe(&tag, [&](const InputEvent&) { ++first_calls; });
        events.signal<InputEvent>().subscribe(&tag, [&](const InputEvent&) { ++second_calls; });

        // Act
        events.signal<InputEvent>().unsubscribe(first);
        events.signal<InputEvent>().emit({.key = Key::A, .is_down = true, .is_repeat = false});
        internal::update_events(events);

        // Assert
        EXPECT_EQ(first_calls, 0);
        EXPECT_EQ(second_calls, 1);
    }

    TEST(Events, MixedSignalsDrainInEmissionOrder)
    {
        // Arrange
        auto events = EventsState();
        auto order = std::vector<int>();
        events.signal<WindowResized>().subscribe(&order, [&](const WindowResized&) { order.push_back(1); });
        events.signal<InputEvent>().subscribe(&order, [&](const InputEvent&) { order.push_back(2); });

        // Act
        events.signal<WindowResized>().emit({.width = 1, .height = 1});
        events.signal<InputEvent>().emit({.key = Key::A, .is_down = true, .is_repeat = false});
        events.signal<WindowResized>().emit({.width = 2, .height = 2});
        internal::update_events(events);

        // Assert
        ASSERT_EQ(order.size(), 3u);
        EXPECT_EQ(order[0], 1);
        EXPECT_EQ(order[1], 2);
        EXPECT_EQ(order[2], 1);
    }

    TEST(Events, InternalRaiseAndOnEventRoundTrip)
    {
        // Arrange: the testable internal API takes the bus explicitly — no global runtime needed.
        auto events = EventsState();
        auto received = 0;
        internal::on_event<WindowResized>(
            events,
            nullptr,
            [&received](const WindowResized& event) { received = event.width; });

        // Act
        internal::raise_event(events, WindowResized {.width = 42, .height = 0});
        internal::update_events(events);

        // Assert
        EXPECT_EQ(received, 42);
    }

    TEST(Events, UnsubscribeAllStopsDeliveryAcrossTheBus)
    {
        // Arrange
        auto events = EventsState();
        const int owner = 0;
        auto calls = 0;
        internal::on_event<WindowResized>(events, &owner, [&calls](const WindowResized&) { ++calls; });

        // Act: bulk-purge the owner, then raise — the handler must not run.
        internal::unsubscribe_all(events, &owner);
        internal::raise_event(events, WindowResized {.width = 1, .height = 1});
        internal::update_events(events);

        // Assert
        EXPECT_EQ(calls, 0);
    }
}
