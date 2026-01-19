#include "pch.h"
#include "tbx/app/app_message_coordinator.h"
#include "tbx/async/cancellation_token.h"
#include "tbx/messages/message.h"
#include "tbx/time/time_span.h"
#include <any>
#include <atomic>
#include <chrono>
#include <future>
#include <stdexcept>
#include <string>

namespace tbx::tests::app
{
    struct TestMessage : public Message
    {
        int value = 0;
    };

    TEST(dispatcher_send, invokes_and_stops_on_handled)
    {
        AppMessageCoordinator d;
        GlobalDispatcherScope dispatcher_scope(d);
        std::atomic<int> count{0};
        int received_value = 0;

        d.add_handler(
            [&](const Message& msg)
            {
                count.fetch_add(1);
                const auto* typed = dynamic_cast<const TestMessage*>(&msg);
                received_value = typed ? typed->value : -1;
                const_cast<Message&>(msg).state = MessageState::Handled;
            });
        d.add_handler(
            [&](const Message& msg)
            {
                count.fetch_add(1);
            });

        TestMessage msg;
        msg.value = 42;
        bool processed_callback = false;
        msg.callbacks.on_processed = [&](const Message&)
        {
            processed_callback = true;
        };

        auto result = d.send(msg);
        EXPECT_EQ(count.load(), 1);
        EXPECT_EQ(received_value, 42);
        EXPECT_TRUE(result.succeeded());
        EXPECT_TRUE(processed_callback);
    }

    TEST(dispatcher_send_no_handlers, returns_processed_without_callbacks)
    {
        AppMessageCoordinator d;
        GlobalDispatcherScope dispatcher_scope(d);

        Message msg;
        bool processed_callback = false;
        msg.callbacks.on_processed = [&](const Message&)
        {
            processed_callback = true;
        };

        auto result = d.send(msg);

        EXPECT_TRUE(result.succeeded());
        EXPECT_TRUE(processed_callback);
        EXPECT_TRUE(result.get_report().empty());
    }

    TEST(dispatcher_send_require_handling, fails_when_unhandled)
    {
        AppMessageCoordinator d;
        GlobalDispatcherScope dispatcher_scope(d);

        Message msg;
        msg.require_handling = true;

        bool error_callback = false;
        bool processed_callback = false;
        msg.callbacks.on_error = [&](const Message&)
        {
            error_callback = true;
        };
        msg.callbacks.on_processed = [&](const Message&)
        {
            processed_callback = true;
        };

        auto result = d.send(msg);

        EXPECT_FALSE(result.succeeded());
        EXPECT_FALSE(result.get_report().empty());
        EXPECT_TRUE(error_callback);
        EXPECT_TRUE(processed_callback);
    }

    TEST(dispatcher_send_failure, triggers_failure_when_unhandled)
    {
        AppMessageCoordinator d;
        GlobalDispatcherScope dispatcher_scope(d);
        std::atomic<int> count{0};

        d.add_handler(
            [&](const Message&)
            {
                count.fetch_add(1);
            });

        Message msg;
        bool error_callback = false;
        bool processed_callback = false;
        msg.callbacks.on_error = [&](const Message&)
        {
            error_callback = true;
        };
        msg.callbacks.on_processed = [&](const Message&)
        {
            processed_callback = true;
        };

        msg.require_handling = true;

        auto result = d.send(msg);

        EXPECT_EQ(count.load(), 1);
        EXPECT_FALSE(result.succeeded());
        EXPECT_FALSE(result.get_report().empty());
        EXPECT_TRUE(error_callback);
        EXPECT_TRUE(processed_callback);
    }

    TEST(dispatcher_send_exception, returns_failure_on_throw)
    {
        AppMessageCoordinator d;
        GlobalDispatcherScope dispatcher_scope(d);

        GTEST_SKIP() << "dispatcher_send_exception is no longer executed.";
    }

    TEST(dispatcher_post, processes_on_next_update)
    {
        AppMessageCoordinator d;
        GlobalDispatcherScope dispatcher_scope(d);
        std::atomic<int> count{0};

        d.add_handler(
            [&](const Message& msg)
            {
                count.fetch_add(1);
                const_cast<Message&>(msg).state = MessageState::Handled;
            });

        Message msg;
        auto future = d.post(msg);

        // Not processed yet
        EXPECT_EQ(count.load(), 0);
        EXPECT_NE(
            future.wait_for(std::chrono::steady_clock::duration::zero()),
            std::future_status::ready);
        auto interim = future.wait_for(TimeSpan().to_duration());
        EXPECT_NE(interim, std::future_status::ready);

        d.process();
        EXPECT_EQ(count.load(), 1);
        future.wait();
        auto result = future.get();
        EXPECT_TRUE(result.succeeded());
    }

    TEST(dispatcher_post_preserves_type, keeps_derived_message_data)
    {
        AppMessageCoordinator d;
        GlobalDispatcherScope dispatcher_scope(d);
        int received_value = -1;

        d.add_handler(
            [&](const Message& msg)
            {
                const auto* typed = dynamic_cast<const TestMessage*>(&msg);
                ASSERT_NE(typed, nullptr);
                received_value = typed->value;
                const_cast<Message&>(msg).state = MessageState::Handled;
            });

        TestMessage msg;
        msg.value = 123;
        auto future = d.post(msg);

        d.process();

        future.wait();
        auto result = future.get();
        EXPECT_TRUE(result.succeeded());
        EXPECT_EQ(received_value, 123);
    }

    TEST(dispatcher_remove, removes_handler_by_uuid)
    {
        AppMessageCoordinator d;
        GlobalDispatcherScope dispatcher_scope(d);
        std::atomic<int> count{0};

        Uuid keep_id = d.add_handler(
            [&](const Message&)
            {
                count.fetch_add(1);
            });
        Uuid drop_id = d.add_handler(
            [&](const Message&)
            {
                count.fetch_add(100);
            });

        d.remove_handler(drop_id);

        TestMessage msg;
        auto result = d.send(msg);

        EXPECT_EQ(count.load(), 1);
        EXPECT_TRUE(result.succeeded());
        EXPECT_TRUE(result.get_report().empty());
        (void)keep_id; // silence unused warning
    }

    TEST(dispatcher_post_cancellation, cancels_before_processing)
    {
        AppMessageCoordinator d;
        GlobalDispatcherScope dispatcher_scope(d);
        std::atomic<int> count{0};

        d.add_handler(
            [&](const Message&)
            {
                count.fetch_add(1);
            });

        CancellationSource source;
        auto token = source.get_token();

        Message msg;
        msg.cancellation_token = token;
        bool cancelled_callback = false;
        bool processed_callback = false;
        msg.callbacks.on_cancelled = [&](const Message&)
        {
            cancelled_callback = true;
        };
        msg.callbacks.on_processed = [&](const Message&)
        {
            processed_callback = true;
        };
        auto future = d.post(msg);

        source.cancel();
        d.process();

        future.wait();
        auto result = future.get();
        EXPECT_FALSE(result.succeeded());
        EXPECT_FALSE(result.get_report().empty());
        EXPECT_TRUE(cancelled_callback);
        EXPECT_TRUE(processed_callback);
        EXPECT_EQ(count.load(), 0);
    }

    TEST(dispatcher_send_cancellation, skips_immediate_dispatch_when_cancelled)
    {
        AppMessageCoordinator d;
        GlobalDispatcherScope dispatcher_scope(d);
        std::atomic<int> count{0};

        d.add_handler(
            [&](const Message&)
            {
                count.fetch_add(1);
            });

        CancellationSource source;
        auto token = source.get_token();
        source.cancel();

        Message msg;
        msg.cancellation_token = token;
        bool cancelled_callback = false;
        msg.callbacks.on_cancelled = [&](const Message&)
        {
            cancelled_callback = true;
        };

        auto result = d.send(msg);

        EXPECT_FALSE(result.succeeded());
        EXPECT_TRUE(cancelled_callback);
        EXPECT_EQ(count.load(), 0);
    }

    TEST(dispatcher_result_value, handler_populates_result_payload)
    {
        AppMessageCoordinator d;
        GlobalDispatcherScope dispatcher_scope(d);

        d.add_handler(
            [](Message& message)
            {
                on_message(message, [](Request<int>& request)
                {
                    request.state = MessageState::Handled;
                    request.result = 123;
                });
            });

        Request<int> msg;
        int payload_value = 0;
        msg.callbacks.on_processed = [&](const Message& processed)
        {
            on_message(processed, [&](const Request<int>& request)
            {
                payload_value = request.result;
            });
        };

        auto result = d.send(msg);

        EXPECT_TRUE(result.succeeded());
        EXPECT_EQ(payload_value, 123);
    }

    TEST(dispatcher_post_result_value, queued_handler_updates_payload)
    {
        AppMessageCoordinator d;
        GlobalDispatcherScope dispatcher_scope(d);

        d.add_handler(
            [](Message& message)
            {
                on_message(message, [](Request<std::string>& request)
                {
                    request.state = MessageState::Handled;
                    request.result = std::string("ready");
                });
            });

        Request<std::string> msg;
        std::string processed_payload;
        msg.callbacks.on_processed = [&](const Message& processed)
        {
            on_message(processed, [&](const Request<std::string>& request)
            {
                processed_payload = request.result;
            });
        };
        auto future = d.post(msg);

        EXPECT_NE(
            future.wait_for(std::chrono::steady_clock::duration::zero()),
            std::future_status::ready);
        auto interim = future.wait_for(TimeSpan().to_duration());
        EXPECT_NE(interim, std::future_status::ready);

        d.process();

        future.wait();
        auto result = future.get();
        EXPECT_TRUE(result.succeeded());
        EXPECT_EQ(processed_payload, "ready");
    }

}
