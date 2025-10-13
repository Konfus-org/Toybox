#include "PCH.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Events/EventCarrier.h"
#include "Tbx/Events/EventListener.h"
#include <string>
#include <vector>

namespace Tbx::Tests::Events
{
    class DummyEvent final : public Event
    {
    public:
        explicit DummyEvent(std::string message)
            : _message(std::move(message))
        {
        }

        std::string ToString() const override { return _message; }

    private:
        std::string _message;
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
            EXPECT_EQ(evt.ToString(), "Hello");
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
            messages.push_back(evt.ToString());
        });

        carrier.Post(DummyEvent("First"));
        carrier.Post(DummyEvent("Second"));
        carrier.Post(DummyEvent("Third"));

        // Act
        bus->Flush();

        EXPECT_TRUE(bus->EventQueue.empty());

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
        auto first = EventBus::Global();
        auto second = EventBus::Global();

        ASSERT_TRUE(first);
        EXPECT_EQ(first.get(), second.get());
        EXPECT_EQ(first->Parent(), nullptr);
    }

    TEST(EventBusTests, LocalBusExtendsGlobalSubscriptions)
    {
        auto globalBus = EventBus::Global();
        EventListener globalListener(globalBus);
        bool invoked = false;

        auto localBus = MakeRef<EventBus>();
        EXPECT_EQ(localBus->Parent().get(), globalBus.get());

        globalListener.Listen<DummyEvent>([&](DummyEvent& evt)
        {
            invoked = true;
            EXPECT_EQ(evt.ToString(), "Global");
            evt.IsHandled = true;
        });

        EventCarrier carrier(localBus);
        const bool handled = carrier.Send(DummyEvent("Global"));

        EXPECT_TRUE(invoked);
        EXPECT_TRUE(handled);
    }

    TEST(EventBusTests, LocalBusFlushDeliversQueuedEventsToGlobalSubscribers)
    {
        auto globalBus = EventBus::Global();
        EventListener globalListener(globalBus);
        bool invoked = false;

        auto localBus = MakeRef<EventBus>();
        EXPECT_EQ(localBus->Parent().get(), globalBus.get());

        globalListener.Listen<DummyEvent>([&](DummyEvent& evt)
        {
            invoked = true;
            EXPECT_EQ(evt.ToString(), "Queued");
        });

        EventCarrier carrier(localBus);
        carrier.Post(DummyEvent("Queued"));

        localBus->Flush();

        EXPECT_TRUE(invoked);
    }
}
