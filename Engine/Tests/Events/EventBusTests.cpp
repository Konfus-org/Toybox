#include "PCH.h"
#include "Tbx/Events/EventBus.h"
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
        EventBus bus;
        bool invoked = false;

        bus.Subscribe<DummyEvent>([&](Event& evt)
        {
            invoked = true;
            auto& dummy = static_cast<DummyEvent&>(evt);
            EXPECT_EQ(dummy.ToString(), "Hello");
            dummy.IsHandled = true;
        });

        // Act
        const bool handled = bus.Send(DummyEvent("Hello"));

        // Assert
        EXPECT_TRUE(invoked);
        EXPECT_TRUE(handled);
    }

    TEST(EventBusTests, Unsubscribe_RemovesListener)
    {
        // Arrange
        EventBus bus;
        bool invoked = false;

        const auto token = bus.Subscribe<DummyEvent>([&](Event&)
        {
            invoked = true;
        });

        // Act
        bus.Unsubscribe(token);
        const bool handled = bus.Send(DummyEvent("Ignored"));

        // Assert
        EXPECT_FALSE(invoked);
        EXPECT_FALSE(handled);
    }

    TEST(EventBusTests, PostAndFlush_DeliversQueuedEventsInOrder)
    {
        // Arrange
        EventBus bus;
        std::vector<std::string> messages;

        bus.Subscribe<DummyEvent>([&](Event& evt)
        {
            auto& dummy = static_cast<DummyEvent&>(evt);
            messages.push_back(dummy.ToString());
        });

        bus.Post(DummyEvent("First"));
        bus.Post(DummyEvent("Second"));
        bus.Post(DummyEvent("Third"));

        // Act
        bus.Flush();
        bus.Flush(); // ensure queue is empty after the first flush

        // Assert
        ASSERT_EQ(messages.size(), 3u);
        EXPECT_EQ(messages[0], "First");
        EXPECT_EQ(messages[1], "Second");
        EXPECT_EQ(messages[2], "Third");
    }

    TEST(EventBusTests, Suppressor_PreventsDispatch)
    {
        // Arrange
        EventBus bus;
        bool invoked = false;

        bus.Subscribe<DummyEvent>([&](Event&)
        {
            invoked = true;
        });

        // Act
        {
            EventSuppressor suppressor;
            bus.Post(DummyEvent("Suppressed"));
            bus.Send(DummyEvent("Suppressed"));
        }

        bus.Flush();

        // Assert
        EXPECT_FALSE(invoked);
    }
}
