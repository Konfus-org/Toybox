#include "PCH.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Events/EventCarrier.h"
#include "Tbx/Events/EventListener.h"
#include <string>
#include <vector>

namespace Tbx::Tests::Events
{
    struct DummyEvent final : public Event
    {
        DummyEvent(std::string message) : Message(message) {}

        const std::string Message;
    };

    TEST(EventBusTests, SubscribeAndSend_InvokesCallback)
    {
        // Arrange
        auto bus = MakeRef<EventBus>();
        EventCarrier carrier(bus);
        EventListener listener(bus);
        bool invoked = false;

        listener.Listen<DummyEvent>([&](DummyEvent& evt)
        {
            invoked = true;
            EXPECT_EQ(evt.Message, "Hello");
            evt.IsHandled = true;
        });

        // Act
        const bool handled = carrier.Send(DummyEvent("Hello"));

        // Assert
        EXPECT_TRUE(invoked);
        EXPECT_TRUE(handled);
    }

    TEST(EventBusTests, Unsubscribe_RemovesListener)
    {
        // Arrange
        auto bus = MakeRef<EventBus>();
        EventCarrier carrier(bus);
        EventListener listener(bus);
        bool invoked = false;

        const auto token = listener.Listen<DummyEvent>([&](DummyEvent&)
        {
            invoked = true;
        });

        // Act
        listener.StopListening(token);
        const bool handled = carrier.Send(DummyEvent("Ignored"));

        // Assert
        EXPECT_FALSE(invoked);
        EXPECT_FALSE(handled);
    }

    TEST(EventBusTests, PostAndFlush_DeliversQueuedEventsInOrder)
    {
        // Arrange
        auto bus = MakeRef<EventBus>();
        EventCarrier carrier(bus);
        EventListener listener(bus);
        std::vector<std::string> messages;

        listener.Listen<DummyEvent>([&](DummyEvent& evt)
        {
            messages.push_back(evt.Message);
        });

        carrier.Post(DummyEvent("First"));
        carrier.Post(DummyEvent("Second"));
        carrier.Post(DummyEvent("Third"));

        // Act
        bus->Flush();

        EXPECT_EQ(bus->PendingEventCount(), 0u);

        // Assert
        ASSERT_EQ(messages.size(), 3u);
        EXPECT_EQ(messages[0], "First");
        EXPECT_EQ(messages[1], "Second");
        EXPECT_EQ(messages[2], "Third");
    }

    TEST(EventBusTests, Suppressor_PreventsDispatch)
    {
        // Arrange
        auto bus = MakeRef<EventBus>();
        EventCarrier carrier(bus);
        EventListener listener(bus);
        bool invoked = false;

        listener.Listen<DummyEvent>([&](DummyEvent&)
        {
            invoked = true;
        });

        // Act
        {
            EventSuppressor suppressor;
            carrier.Post(DummyEvent("Suppressed"));
            carrier.Send(DummyEvent("Suppressed"));
            bus->Flush();
        }

        bus->Flush();

        // Assert
        EXPECT_FALSE(invoked);
    }

    TEST(EventBusTests, Global_ReturnsSingletonInstance)
    {
        auto first = EventBus::Global;
        auto second = EventBus::Global;

        ASSERT_TRUE(first);
        EXPECT_EQ(first.get(), second.get());
    }

    TEST(EventBusTests, ChildBusDoesNotPropagateEventsToParent)
    {
        auto globalBus = EventBus::Global;
        EventListener globalListener(globalBus);
        bool invoked = false;

        auto localBus = MakeRef<EventBus>(globalBus);

        globalListener.Listen<DummyEvent>([&](DummyEvent& evt)
        {
            invoked = true;
        });

        EventCarrier carrier(localBus);
        const bool handled = carrier.Send(DummyEvent("Global"));

        EXPECT_FALSE(invoked);
        EXPECT_FALSE(handled);
    }

    TEST(EventBusTests, ParentFlushPropagatesQueuedEventsToChildren)
    {
        auto parentBus = MakeRef<EventBus>();
        auto childBus = MakeRef<EventBus>(parentBus);
        EventListener childListener(childBus);
        bool invoked = false;

        childListener.Listen<DummyEvent>([&](DummyEvent& evt)
        {
            invoked = true;
            EXPECT_EQ(evt.Message, "Queued");
        });

        EventCarrier carrier(parentBus);
        carrier.Post(DummyEvent("Queued"));

        parentBus->Flush();

        EXPECT_TRUE(invoked);
    }

    TEST(EventBusTests, ParentPostedEventsRemainOnParentQueue)
    {
        auto parentBus = MakeRef<EventBus>();
        auto childBus = MakeRef<EventBus>(parentBus);
        EventListener childListener(childBus);
        bool invoked = false;

        childListener.Listen<DummyEvent>([&](DummyEvent& evt)
        {
            invoked = true;
            EXPECT_EQ(evt.Message, "Cascade");
        });

        EventCarrier carrier(parentBus);
        carrier.Post(DummyEvent("Cascade"));

        EXPECT_EQ(childBus->PendingEventCount(), 0u);

        childBus->Flush();

        EXPECT_FALSE(invoked);

        parentBus->Flush();

        EXPECT_TRUE(invoked);
    }

    TEST(EventBusTests, ParentSendPropagatesToChildDecorators)
    {
        auto parentBus = MakeRef<EventBus>();
        auto childBus = MakeRef<EventBus>(parentBus);
        EventListener childListener(childBus);
        bool invoked = false;

        childListener.Listen<DummyEvent>([&](DummyEvent& evt)
        {
            invoked = true;
            EXPECT_EQ(evt.Message, "Direct");
            evt.IsHandled = true;
        });

        EventCarrier carrier(parentBus);
        const bool handled = carrier.Send(DummyEvent("Direct"));

        EXPECT_TRUE(invoked);
        EXPECT_TRUE(handled);
    }

    TEST(EventBusTests, GlobalBusPropagatesEventsToAllChildren)
    {
        auto globalBus = EventBus::Global;
        auto childBus = MakeRef<EventBus>(globalBus);
        EventListener childListener(childBus);
        std::vector<std::string> messages;

        childListener.Listen<DummyEvent>([&](DummyEvent& evt)
        {
            messages.push_back(evt.Message);
            if (evt.Message == "GlobalSend")
            {
                evt.IsHandled = true;
            }
        });

        EventCarrier carrier(globalBus);
        const bool handled = carrier.Send(DummyEvent("GlobalSend"));

        ASSERT_EQ(messages.size(), 1u);
        EXPECT_TRUE(handled);
        EXPECT_EQ(messages[0], "GlobalSend");

        carrier.Post(DummyEvent("GlobalPost"));

        globalBus->Flush();

        ASSERT_EQ(messages.size(), 2u);
        EXPECT_EQ(messages[1], "GlobalPost");
    }

    TEST(EventBusTests, ChildBusRemovesItselfOnDestruction)
    {
        auto parentBus = MakeRef<EventBus>();

        int invocationCount = 0;

        {
            auto childBus = MakeRef<EventBus>(parentBus);
            EventListener childListener(childBus);
            childListener.Listen<DummyEvent>([&](DummyEvent&)
            {
                invocationCount++;
            });

            EventCarrier carrier(parentBus);
            carrier.Send(DummyEvent("Scoped"));
        }

        EventCarrier carrier(parentBus);
        carrier.Send(DummyEvent("After"));
        EXPECT_EQ(invocationCount, 1);
    }

    TEST(EventBusTests, ParentDestructionReparentsChildrenToGrandparent)
    {
        Ref<EventBus> childBus = nullptr;

        {
            auto grandParentBus = MakeRef<EventBus>();
            auto parentBus = MakeRef<EventBus>(grandParentBus);

            childBus = MakeRef<EventBus>(parentBus);
        }

        ASSERT_TRUE(childBus);
        auto globalBus = EventBus::Global;

        bool invoked = false;
        EventListener listener(childBus);
        listener.Listen<DummyEvent>([&](DummyEvent& evt)
        {
            invoked = true;
        });

        EventCarrier carrier(globalBus);
        carrier.Post(DummyEvent("Reparented"));
        globalBus->Flush();

        EXPECT_TRUE(invoked);
    }
}

