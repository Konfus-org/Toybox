#include "pch.h"
#include "tbx/messages/coordinator.h"
#include "tbx/messages/message.h"
#include "tbx/messages/handler.h"
#include <atomic>

namespace tbx::tests::messages
{
    struct TestMessage : public ::tbx::Message
    {
        int value = 0;
    };

    TEST(dispatcher_send, invokes_and_stops_on_handled)
    {
        ::tbx::MessageCoordinator d;
        std::atomic<int> count{0};

        d.add_handler([&](const ::tbx::Message& msg)
        {
            count.fetch_add(1);
            const_cast<::tbx::Message&>(msg).is_handled = true;
        });
        d.add_handler([&](const ::tbx::Message& msg)
        {
            count.fetch_add(1);
        });

        TestMessage msg;
        msg.value = 42;
        d.send(msg);
        EXPECT_EQ(count.load(), 1);
    }

    TEST(dispatcher_post, processes_on_next_update)
    {
        ::tbx::MessageCoordinator d;
        std::atomic<int> count{0};

        d.add_handler([&](const ::tbx::Message&)
        {
            count.fetch_add(1);
        });

        ::tbx::Message msg;
        d.post(msg);

        // Not processed yet
        EXPECT_EQ(count.load(), 0);

        d.process();
        EXPECT_EQ(count.load(), 1);
    }

    TEST(dispatcher_remove, removes_handler_by_uuid)
    {
        ::tbx::MessageCoordinator d;
        std::atomic<int> count{0};

        ::tbx::Uuid keep_id = d.add_handler([&](const ::tbx::Message&)
        {
            count.fetch_add(1);
        });
        ::tbx::Uuid drop_id = d.add_handler([&](const ::tbx::Message&)
        {
            count.fetch_add(100);
        });

        d.remove_handler(drop_id);

        TestMessage msg;
        d.send(msg);

        EXPECT_EQ(count.load(), 1);
        (void)keep_id; // silence unused warning
    }
}
