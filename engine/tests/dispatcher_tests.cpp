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

    struct CountingHandler : public ::tbx::IMessageHandler
    {
        std::atomic<int>* counter = nullptr;
        bool stop = false;
        void on_message(const ::tbx::Message& msg) override
        {
            if (counter) counter->fetch_add(1);
            if (stop)
            {
                const_cast<::tbx::Message&>(msg).is_handled = true;
            }
        }
    };

    TEST(dispatcher_send, invokes_and_stops_on_handled)
    {
        ::tbx::MessageCoordinator d;
        std::atomic<int> count{0};

        CountingHandler h1; h1.counter = &count; h1.stop = true;
        CountingHandler h2; h2.counter = &count; h2.stop = false;
        d.add_handler(h1);
        d.add_handler(h2);

        TestMessage msg;
        msg.value = 42;
        d.send(msg);
        EXPECT_EQ(count.load(), 1);
    }

    TEST(dispatcher_post, processes_on_next_update)
    {
        ::tbx::MessageCoordinator d;
        std::atomic<int> count{0};

        struct CountHandler : public ::tbx::IMessageHandler
        {
            std::atomic<int>* counter = nullptr;
            void on_message(const ::tbx::Message&) override
            {
                if (counter) counter->fetch_add(1);
            }
        } h;
        h.counter = &count;
        d.add_handler(h);

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

        struct IncHandler : public ::tbx::IMessageHandler
        {
            std::atomic<int>* counter = nullptr;
            int add = 0;
            void on_message(const ::tbx::Message&) override
            {
                if (counter) counter->fetch_add(add);
            }
        } hKeep, hDrop;
        hKeep.counter = &count; hKeep.add = 1;
        hDrop.counter = &count; hDrop.add = 100;
        ::tbx::Uuid keep_id = d.add_handler(hKeep);
        ::tbx::Uuid drop_id = d.add_handler(hDrop);

        d.remove_handler(drop_id);

        TestMessage msg;
        d.send(msg);

        EXPECT_EQ(count.load(), 1);
        (void)keep_id; // silence unused warning
    }
}
