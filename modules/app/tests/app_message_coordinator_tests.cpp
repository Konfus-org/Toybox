#include "pch.h"
#include "tbx/app/app_message_coordinator.h"
#include "tbx/messages/message.h"
#include "tbx/messages/handler.h"
#include "tbx/async/cancellation_token.h"
#include "tbx/time/time_span.h"
#include <any>
#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <typeinfo>

namespace tbx::tests::app
{
    struct TestMessage : public Message
    {
        int value = 0;
    };

    TEST(dispatcher_send, invokes_and_stops_on_handled)
    {
        AppMessageCoordinator d;
        std::atomic<int> count{0};

        d.add_handler([&](const Message& msg)
        {
            count.fetch_add(1);
            const_cast<Message&>(msg).state = MessageState::Handled;
        });
        d.add_handler([&](const Message& msg)
        {
            count.fetch_add(1);
        });

        TestMessage msg;
        msg.value = 42;
        bool handled_callback = false;
        bool processed_callback = false;
        msg.callbacks.on_handled = [&](const Message&)
        {
            handled_callback = true;
        };
        msg.callbacks.on_processed = [&](const Message&)
        {
            processed_callback = true;
        };

        auto result = d.send(msg);
        EXPECT_EQ(count.load(), 1);
        EXPECT_EQ(msg.state, MessageState::Handled);
        EXPECT_TRUE(result.succeeded());
        EXPECT_TRUE(handled_callback);
        EXPECT_TRUE(processed_callback);
    }

    TEST(dispatcher_send_no_handlers, returns_processed_without_callbacks)
    {
        AppMessageCoordinator d;

        Message msg;
        bool processed_callback = false;
        msg.callbacks.on_processed = [&](const Message&)
        {
            processed_callback = true;
        };

        auto result = d.send(msg);

        EXPECT_EQ(msg.state, MessageState::Processed);
        EXPECT_TRUE(result.succeeded());
        EXPECT_TRUE(processed_callback);
        EXPECT_TRUE(result.get_report().empty());
    }

    TEST(dispatcher_send_require_handling, fails_when_unhandled)
    {
        AppMessageCoordinator d;

        Message msg;
        msg.require_handling = true;

        bool failure_callback = false;
        bool processed_callback = false;
        msg.callbacks.on_failure = [&](const Message&)
        {
            failure_callback = true;
        };
        msg.callbacks.on_processed = [&](const Message&)
        {
            processed_callback = true;
        };

        auto result = d.send(msg);

        EXPECT_EQ(msg.state, MessageState::Failed);
        EXPECT_FALSE(result.succeeded());
        EXPECT_FALSE(result.get_report().empty());
        EXPECT_TRUE(failure_callback);
        EXPECT_TRUE(processed_callback);
    }

    TEST(dispatcher_send_failure, triggers_failure_when_unhandled)
    {
        AppMessageCoordinator d;
        std::atomic<int> count{0};

        d.add_handler([&](const Message&)
        {
            count.fetch_add(1);
        });

        Message msg;
        bool failure_callback = false;
        bool processed_callback = false;
        msg.callbacks.on_failure = [&](const Message&)
        {
            failure_callback = true;
        };
        msg.callbacks.on_processed = [&](const Message&)
        {
            processed_callback = true;
        };

        msg.require_handling = true;

        auto result = d.send(msg);

        EXPECT_EQ(count.load(), 1);
        EXPECT_EQ(msg.state, MessageState::Failed);
        EXPECT_FALSE(result.succeeded());
        EXPECT_FALSE(result.get_report().empty());
        EXPECT_TRUE(failure_callback);
        EXPECT_TRUE(processed_callback);
    }

    TEST(dispatcher_send_exception, returns_failure_on_throw)
    {
        AppMessageCoordinator d;

        d.add_handler([](const Message&)
        {
            throw std::runtime_error("send failure");
        });

        Message msg;
        bool failure_callback = false;
        bool processed_callback = false;
        msg.callbacks.on_failure = [&](const Message&)
        {
            failure_callback = true;
        };
        msg.callbacks.on_processed = [&](const Message&)
        {
            processed_callback = true;
        };

        auto result = d.send(msg);

        EXPECT_EQ(msg.state, MessageState::Failed);
        EXPECT_FALSE(result.succeeded());
        EXPECT_TRUE(failure_callback);
        EXPECT_TRUE(processed_callback);
        EXPECT_FALSE(result.get_report().empty());
    }

    TEST(dispatcher_send_with_delay, returns_failure_for_incompatible_config)
    {
        AppMessageCoordinator d;

        Message msg;
        msg.delay_in_ticks = static_cast<std::size_t>(1);
        bool processed_callback = false;
        msg.callbacks.on_processed = [&](const Message&)
        {
            processed_callback = true;
        };

        auto result = d.send(msg);

        EXPECT_EQ(msg.state, MessageState::Processed);
        EXPECT_TRUE(result.succeeded());
        EXPECT_TRUE(result.get_report().empty());
        EXPECT_TRUE(processed_callback);
    }

    TEST(dispatcher_post, processes_on_next_update)
    {
        AppMessageCoordinator d;
        std::atomic<int> count{0};

        d.add_handler([&](const Message& msg)
        {
            count.fetch_add(1);
            const_cast<Message&>(msg).state = MessageState::Handled;
        });

        Message msg;
        auto result = d.post(msg);

        // Not processed yet
        EXPECT_EQ(count.load(), 0);
        EXPECT_FALSE(result.succeeded());
        EXPECT_TRUE(result.get_report().empty());

        d.process();
        EXPECT_EQ(count.load(), 1);
        EXPECT_TRUE(result.succeeded());
    }

    TEST(dispatcher_remove, removes_handler_by_uuid)
    {
        AppMessageCoordinator d;
        std::atomic<int> count{0};

        Uuid keep_id = d.add_handler([&](const Message&)
        {
            count.fetch_add(1);
        });
        Uuid drop_id = d.add_handler([&](const Message&)
        {
            count.fetch_add(100);
        });

        d.remove_handler(drop_id);

        TestMessage msg;
        auto result = d.send(msg);

        EXPECT_EQ(count.load(), 1);
        EXPECT_EQ(msg.state, MessageState::Processed);
        EXPECT_TRUE(result.succeeded());
        EXPECT_TRUE(result.get_report().empty());
        (void)keep_id; // silence unused warning
    }

    TEST(dispatcher_post_tick_delay, delays_processing_by_ticks)
    {
        AppMessageCoordinator d;
        std::atomic<int> count{0};

        d.add_handler([&](const Message& msg)
        {
            count.fetch_add(1);
            const_cast<Message&>(msg).state = MessageState::Handled;
        });

        Message msg;
        msg.delay_in_ticks = static_cast<std::size_t>(1);
        auto result = d.post(msg);

        EXPECT_FALSE(result.succeeded());
        EXPECT_TRUE(result.get_report().empty());
        d.process();
        EXPECT_FALSE(result.succeeded());
        EXPECT_EQ(count.load(), 0);

        d.process();
        EXPECT_TRUE(result.succeeded());
        EXPECT_EQ(count.load(), 1);
    }

    TEST(dispatcher_post_combined_delay, fails_when_both_delays_requested)
    {
        using namespace std::chrono_literals;

        AppMessageCoordinator d;
        std::atomic<int> count{0};

        d.add_handler([&](const Message& msg)
        {
            count.fetch_add(1);
            const_cast<Message&>(msg).state = MessageState::Handled;
        });

        Message msg;
        msg.delay_in_ticks = static_cast<std::size_t>(1);
        TimeSpan span;
        span.milliseconds = 5;
        msg.delay_in_seconds = span;
        bool processed_callback = false;
        msg.callbacks.on_processed = [&](const Message&)
        {
            processed_callback = true;
        };

        auto result = d.post(msg);

        EXPECT_FALSE(result.succeeded());
        EXPECT_TRUE(result.get_report().empty());
        EXPECT_TRUE(processed_callback);
        d.process();
        EXPECT_FALSE(result.succeeded());
        EXPECT_EQ(count.load(), 0);

        d.process();
        EXPECT_TRUE(result.succeeded());
        EXPECT_EQ(count.load(), 1);
    }

    TEST(dispatcher_post_exception, returns_failure_on_throw)
    {
        AppMessageCoordinator d;

        d.add_handler([](const Message&)
        {
            throw std::runtime_error("post failure");
        });

        Message msg;
        bool failure_callback = false;
        bool processed_callback = false;
        msg.callbacks.on_failure = [&](const Message&)
        {
            failure_callback = true;
        };
        msg.callbacks.on_processed = [&](const Message&)
        {
            processed_callback = true;
        };
        auto result = d.post(msg);

        d.process();

        EXPECT_EQ(msg.state, MessageState::InProgress);
        EXPECT_FALSE(result.succeeded());
        EXPECT_TRUE(failure_callback);
        EXPECT_TRUE(processed_callback);
        EXPECT_FALSE(result.get_report().empty());
    }

    TEST(dispatcher_post_time_delay, delays_processing_by_timespan)
    {
        using namespace std::chrono_literals;

        AppMessageCoordinator d;
        std::atomic<int> count{0};

        d.add_handler([&](const Message& msg)
        {
            count.fetch_add(1);
            const_cast<Message&>(msg).state = MessageState::Handled;
        });

        Message msg;
        TimeSpan delay;
        delay.milliseconds = 5;
        msg.delay_in_seconds = delay;
        auto result = d.post(msg);

        EXPECT_FALSE(result.succeeded());
        d.process();
        EXPECT_FALSE(result.succeeded());
        EXPECT_EQ(count.load(), 0);

        std::this_thread::sleep_for(6ms);

        d.process();
        EXPECT_TRUE(result.succeeded());
        EXPECT_EQ(count.load(), 1);
    }

    TEST(dispatcher_post_cancellation, cancels_before_processing)
    {
        AppMessageCoordinator d;
        std::atomic<int> count{0};

        d.add_handler([&](const Message&)
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
        auto result = d.post(msg);

        source.cancel();
        d.process();

        EXPECT_FALSE(result.succeeded());
        EXPECT_FALSE(result.get_report().empty());
        EXPECT_TRUE(cancelled_callback);
        EXPECT_TRUE(processed_callback);
        EXPECT_EQ(count.load(), 0);
    }

    TEST(dispatcher_send_cancellation, skips_immediate_dispatch_when_cancelled)
    {
        AppMessageCoordinator d;
        std::atomic<int> count{0};

        d.add_handler([&](const Message&)
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

        EXPECT_EQ(msg.state, MessageState::Cancelled);
        EXPECT_FALSE(result.succeeded());
        EXPECT_TRUE(cancelled_callback);
        EXPECT_EQ(count.load(), 0);
    }

    TEST(dispatcher_result_value, handler_populates_result_payload)
    {
        AppMessageCoordinator d;

        d.add_handler([](const Message& message)
        {
            auto& mutable_msg = const_cast<Message&>(message);
            mutable_msg.state = MessageState::Handled;
            mutable_msg.payload = 123;
        });

        Message msg;
        auto result = d.send(msg);

        EXPECT_EQ(msg.state, MessageState::Handled);
        EXPECT_TRUE(result.succeeded());
        EXPECT_TRUE(msg.payload.has_value());
        EXPECT_NE(std::any_cast<int>(&msg.payload), nullptr);
        EXPECT_EQ(std::any_cast<int>(msg.payload), 123);
        EXPECT_EQ(std::any_cast<float>(&msg.payload), nullptr);
    }

    TEST(dispatcher_post_result_value, queued_handler_updates_payload)
    {
        AppMessageCoordinator d;

        d.add_handler([](const Message& message)
        {
            auto& mutable_msg = const_cast<Message&>(message);
            mutable_msg.state = MessageState::Handled;
            mutable_msg.payload = std::string("ready");
        });

        Message msg;
        std::string processed_payload;
        msg.callbacks.on_processed = [&](const Message& processed)
        {
            EXPECT_TRUE(processed.payload.has_value());
            const std::string* value = std::any_cast<std::string>(&processed.payload);
            processed_payload = value ? *value : std::string();
        };
        auto result = d.post(msg);

        EXPECT_FALSE(result.succeeded());
        EXPECT_FALSE(msg.payload.has_value());

        d.process();

        EXPECT_TRUE(result.succeeded());
        EXPECT_FALSE(msg.payload.has_value());
        EXPECT_EQ(processed_payload, "ready");
    }

    TEST(dispatcher_send_timeout, marks_result_as_timed_out)
    {
        AppMessageCoordinator d;
        d.add_handler([](const Message&)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        });

        Message msg;
        TimeSpan timeout;
        timeout.milliseconds = 1;
        msg.timeout = timeout;
        bool timeout_callback = false;
        msg.callbacks.on_timeout = [&](const Message&)
        {
            timeout_callback = true;
        };

        auto result = d.send(msg);
        EXPECT_EQ(msg.state, MessageState::TimedOut);
        EXPECT_FALSE(result.succeeded());
        EXPECT_TRUE(timeout_callback);
    }

    TEST(dispatcher_post_timeout, cancels_message_before_delivery)
    {
        AppMessageCoordinator d;
        d.add_handler([](const Message&) {});

        Message msg;
        TimeSpan delay;
        delay.milliseconds = 50;
        msg.delay_in_seconds = delay;
        TimeSpan timeout;
        timeout.milliseconds = 1;
        msg.timeout = timeout;
        bool timeout_callback = false;
        msg.callbacks.on_timeout = [&](const Message&)
        {
            timeout_callback = true;
        };

        auto result = d.post(msg);
        EXPECT_FALSE(result.succeeded());
        EXPECT_TRUE(result.get_report().empty());

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        d.process();

        EXPECT_FALSE(result.succeeded());
        EXPECT_FALSE(result.get_report().empty());
        EXPECT_TRUE(timeout_callback);
    }
}
