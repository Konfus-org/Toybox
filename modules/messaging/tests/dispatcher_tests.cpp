#include "pch.h"
#include "tbx/messages/coordinator.h"
#include "tbx/messages/message.h"
#include "tbx/messages/handler.h"
#include "tbx/messages/cancellation_token.h"
#include "tbx/std/casting.h"
#include "tbx/time/time_span.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <thread>

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
            const_cast<::tbx::Message&>(msg).state = ::tbx::MessageState::Handled;
        });
        d.add_handler([&](const ::tbx::Message& msg)
        {
            count.fetch_add(1);
        });

        TestMessage msg;
        msg.value = 42;
        bool handled_callback = false;
        bool processed_callback = false;
        msg.callbacks.on_handled = [&](const ::tbx::Message&)
        {
            handled_callback = true;
        };
        msg.callbacks.on_processed = [&](const ::tbx::Message&)
        {
            processed_callback = true;
        };

        auto result = d.send(msg);
        EXPECT_EQ(count.load(), 1);
        EXPECT_EQ(msg.state, ::tbx::MessageState::Handled);
        EXPECT_TRUE(result.succeeded());
        EXPECT_TRUE(handled_callback);
        EXPECT_TRUE(processed_callback);
    }

    TEST(dispatcher_send_no_handlers, returns_processed_without_callbacks)
    {
        ::tbx::MessageCoordinator d;

        ::tbx::Message msg;
        bool processed_callback = false;
        msg.callbacks.on_processed = [&](const ::tbx::Message&)
        {
            processed_callback = true;
        };

        auto result = d.send(msg);

        EXPECT_EQ(msg.state, ::tbx::MessageState::Processed);
        EXPECT_TRUE(result.succeeded());
        EXPECT_TRUE(processed_callback);
        EXPECT_TRUE(result.get_report().is_empty());
    }

    TEST(dispatcher_send_failure, triggers_failure_when_unhandled)
    {
        ::tbx::MessageCoordinator d;
        std::atomic<int> count{0};

        d.add_handler([&](const ::tbx::Message&)
        {
            count.fetch_add(1);
        });

        ::tbx::Message msg;
        bool failure_callback = false;
        bool processed_callback = false;
        msg.callbacks.on_failure = [&](const ::tbx::Message&)
        {
            failure_callback = true;
        };
        msg.callbacks.on_processed = [&](const ::tbx::Message&)
        {
            processed_callback = true;
        };

        auto result = d.send(msg);

        EXPECT_EQ(count.load(), 1);
        EXPECT_EQ(msg.state, ::tbx::MessageState::Failed);
        EXPECT_FALSE(result.succeeded());
        EXPECT_FALSE(result.get_report().is_empty());
        EXPECT_TRUE(failure_callback);
        EXPECT_TRUE(processed_callback);
    }

    TEST(dispatcher_send_exception, returns_failure_on_throw)
    {
        ::tbx::MessageCoordinator d;

        d.add_handler([](const ::tbx::Message&)
        {
            throw std::runtime_error("send failure");
        });

        ::tbx::Message msg;
        bool failure_callback = false;
        bool processed_callback = false;
        msg.callbacks.on_failure = [&](const ::tbx::Message&)
        {
            failure_callback = true;
        };
        msg.callbacks.on_processed = [&](const ::tbx::Message&)
        {
            processed_callback = true;
        };

        auto result = d.send(msg);

        EXPECT_EQ(msg.state, ::tbx::MessageState::Failed);
        EXPECT_FALSE(result.succeeded());
        EXPECT_TRUE(failure_callback);
        EXPECT_TRUE(processed_callback);
        EXPECT_FALSE(result.get_report().is_empty());
    }

    TEST(dispatcher_send_with_delay, returns_failure_for_incompatible_config)
    {
        ::tbx::MessageCoordinator d;

        ::tbx::Message msg;
        msg.delay_in_ticks = static_cast<std::size_t>(1);
#ifdef TBX_ASSERTS_ENABLED
        EXPECT_DEBUG_DEATH(d.send(msg), ".*");
#else
        bool failure_callback = false;
        bool processed_callback = false;
        msg.callbacks.on_failure = [&](const ::tbx::Message&)
        {
            failure_callback = true;
        };
        msg.callbacks.on_processed = [&](const ::tbx::Message&)
        {
            processed_callback = true;
        };

        auto result = d.send(msg);

        EXPECT_EQ(msg.state, ::tbx::MessageState::Failed);
        EXPECT_FALSE(result.succeeded());
        EXPECT_TRUE(failure_callback);
        EXPECT_TRUE(processed_callback);
        EXPECT_FALSE(result.get_report().is_empty());
#endif
    }

    TEST(dispatcher_post, processes_on_next_update)
    {
        ::tbx::MessageCoordinator d;
        std::atomic<int> count{0};

        d.add_handler([&](const ::tbx::Message& msg)
        {
            count.fetch_add(1);
            const_cast<::tbx::Message&>(msg).state = ::tbx::MessageState::Handled;
        });

        ::tbx::Message msg;
        auto result = d.post(msg);

        // Not processed yet
        EXPECT_EQ(count.load(), 0);
        EXPECT_FALSE(result.succeeded());
        EXPECT_TRUE(result.get_report().is_empty());

        d.process();
        EXPECT_EQ(count.load(), 1);
        EXPECT_TRUE(result.succeeded());
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
        auto result = d.send(msg);

        EXPECT_EQ(count.load(), 1);
        EXPECT_EQ(msg.state, ::tbx::MessageState::Failed);
        EXPECT_FALSE(result.succeeded());
        EXPECT_FALSE(result.get_report().is_empty());
        (void)keep_id; // silence unused warning
    }

    TEST(dispatcher_post_tick_delay, delays_processing_by_ticks)
    {
        ::tbx::MessageCoordinator d;
        std::atomic<int> count{0};

        d.add_handler([&](const ::tbx::Message& msg)
        {
            count.fetch_add(1);
            const_cast<::tbx::Message&>(msg).state = ::tbx::MessageState::Handled;
        });

        ::tbx::Message msg;
        msg.delay_in_ticks = static_cast<std::size_t>(1);
        auto result = d.post(msg);

        EXPECT_FALSE(result.succeeded());
        EXPECT_TRUE(result.get_report().is_empty());
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

        ::tbx::MessageCoordinator d;
        std::atomic<int> count{0};

        d.add_handler([&](const ::tbx::Message& msg)
        {
            count.fetch_add(1);
            const_cast<::tbx::Message&>(msg).state = ::tbx::MessageState::Handled;
        });

        ::tbx::Message msg;
        msg.delay_in_ticks = static_cast<std::size_t>(1);
        ::tbx::TimeSpan span;
        span.milliseconds = 5;
        msg.delay_in_seconds = span;
#ifdef TBX_ASSERTS_ENABLED
        EXPECT_DEBUG_DEATH(d.post(msg), ".*");
#else
        bool failure_callback = false;
        bool processed_callback = false;
        msg.callbacks.on_failure = [&](const ::tbx::Message&)
        {
            failure_callback = true;
        };
        msg.callbacks.on_processed = [&](const ::tbx::Message&)
        {
            processed_callback = true;
        };

        auto result = d.post(msg);

        EXPECT_EQ(msg.state, ::tbx::MessageState::Failed);
        EXPECT_FALSE(result.succeeded());
        EXPECT_TRUE(failure_callback);
        EXPECT_TRUE(processed_callback);
        EXPECT_FALSE(result.get_report().is_empty());
        d.process();
        EXPECT_EQ(count.load(), 0);
#endif
    }

    TEST(dispatcher_post_exception, returns_failure_on_throw)
    {
        ::tbx::MessageCoordinator d;

        d.add_handler([](const ::tbx::Message&)
        {
            throw std::runtime_error("post failure");
        });

        ::tbx::Message msg;
        bool failure_callback = false;
        bool processed_callback = false;
        msg.callbacks.on_failure = [&](const ::tbx::Message&)
        {
            failure_callback = true;
        };
        msg.callbacks.on_processed = [&](const ::tbx::Message&)
        {
            processed_callback = true;
        };
        auto result = d.post(msg);

        d.process();

        EXPECT_EQ(msg.state, ::tbx::MessageState::Failed);
        EXPECT_FALSE(result.succeeded());
        EXPECT_TRUE(failure_callback);
        EXPECT_TRUE(processed_callback);
        EXPECT_FALSE(result.get_report().is_empty());
    }

    TEST(dispatcher_post_time_delay, delays_processing_by_timespan)
    {
        using namespace std::chrono_literals;

        ::tbx::MessageCoordinator d;
        std::atomic<int> count{0};

        d.add_handler([&](const ::tbx::Message& msg)
        {
            count.fetch_add(1);
            const_cast<::tbx::Message&>(msg).state = ::tbx::MessageState::Handled;
        });

        ::tbx::Message msg;
        ::tbx::TimeSpan delay;
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
        ::tbx::MessageCoordinator d;
        std::atomic<int> count{0};

        d.add_handler([&](const ::tbx::Message&)
        {
            count.fetch_add(1);
        });

        ::tbx::CancellationSource source;
        auto token = source.get_token();

        ::tbx::Message msg;
        msg.cancellation_token = token;
        bool cancelled_callback = false;
        bool processed_callback = false;
        msg.callbacks.on_cancelled = [&](const ::tbx::Message&)
        {
            cancelled_callback = true;
        };
        msg.callbacks.on_processed = [&](const ::tbx::Message&)
        {
            processed_callback = true;
        };
        auto result = d.post(msg);

        source.cancel();
        d.process();

        EXPECT_FALSE(result.succeeded());
        EXPECT_FALSE(result.get_report().is_empty());
        EXPECT_TRUE(cancelled_callback);
        EXPECT_TRUE(processed_callback);
        EXPECT_EQ(count.load(), 0);
    }

    TEST(dispatcher_send_cancellation, skips_immediate_dispatch_when_cancelled)
    {
        ::tbx::MessageCoordinator d;
        std::atomic<int> count{0};

        d.add_handler([&](const ::tbx::Message&)
        {
            count.fetch_add(1);
        });

        ::tbx::CancellationSource source;
        auto token = source.get_token();
        source.cancel();

        ::tbx::Message msg;
        msg.cancellation_token = token;
        bool cancelled_callback = false;
        msg.callbacks.on_cancelled = [&](const ::tbx::Message&)
        {
            cancelled_callback = true;
        };

        auto result = d.send(msg);

        EXPECT_EQ(msg.state, ::tbx::MessageState::Cancelled);
        EXPECT_FALSE(result.succeeded());
        EXPECT_TRUE(cancelled_callback);
        EXPECT_EQ(count.load(), 0);
    }

    TEST(dispatcher_result_value, handler_populates_result_payload)
    {
        ::tbx::MessageCoordinator d;

        d.add_handler([](const ::tbx::Message& message)
        {
            auto& mutable_msg = const_cast<::tbx::Message&>(message);
            mutable_msg.state = ::tbx::MessageState::Handled;
            mutable_msg.payload = 123;
        });

        ::tbx::Message msg;
        auto result = d.send(msg);

        EXPECT_EQ(msg.state, ::tbx::MessageState::Handled);
        EXPECT_TRUE(result.succeeded());
        EXPECT_TRUE(msg.payload.has_value());
        EXPECT_TRUE(tbx::is<int>(msg.payload));
        EXPECT_EQ(msg.payload.get_value<int>(), 123);
        EXPECT_FALSE(tbx::is<float>(msg.payload));
    }

    TEST(dispatcher_post_result_value, queued_handler_updates_payload)
    {
        ::tbx::MessageCoordinator d;

        d.add_handler([](const ::tbx::Message& message)
        {
            auto& mutable_msg = const_cast<::tbx::Message&>(message);
            mutable_msg.state = ::tbx::MessageState::Handled;
            mutable_msg.payload = std::string("ready");
        });

        ::tbx::Message msg;
        auto result = d.post(msg);

        EXPECT_FALSE(result.succeeded());
        EXPECT_FALSE(msg.payload.has_value());

        d.process();

        EXPECT_TRUE(result.succeeded());
        EXPECT_TRUE(msg.payload.has_value());
        EXPECT_TRUE(tbx::is<std::string>(msg.payload));
        EXPECT_EQ(msg.payload.get_value<std::string>(), "ready");
    }

    TEST(dispatcher_send_timeout, marks_result_as_timed_out)
    {
        ::tbx::MessageCoordinator d;
        d.add_handler([](const ::tbx::Message&)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        });

        ::tbx::Message msg;
        ::tbx::TimeSpan timeout;
        timeout.milliseconds = 1;
        msg.timeout = timeout;
        bool timeout_callback = false;
        msg.callbacks.on_timeout = [&](const ::tbx::Message&)
        {
            timeout_callback = true;
        };

        auto result = d.send(msg);
        EXPECT_EQ(msg.state, ::tbx::MessageState::TimedOut);
        EXPECT_FALSE(result.succeeded());
        EXPECT_TRUE(timeout_callback);
    }

    TEST(dispatcher_post_timeout, cancels_message_before_delivery)
    {
        ::tbx::MessageCoordinator d;
        d.add_handler([](const ::tbx::Message&) {});

        ::tbx::Message msg;
        ::tbx::TimeSpan delay;
        delay.milliseconds = 50;
        msg.delay_in_seconds = delay;
        ::tbx::TimeSpan timeout;
        timeout.milliseconds = 1;
        msg.timeout = timeout;
        bool timeout_callback = false;
        msg.callbacks.on_timeout = [&](const ::tbx::Message&)
        {
            timeout_callback = true;
        };

        auto result = d.post(msg);
        EXPECT_FALSE(result.succeeded());
        EXPECT_TRUE(result.get_report().is_empty());

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        d.process();

        EXPECT_FALSE(result.succeeded());
        EXPECT_FALSE(result.get_report().is_empty());
        EXPECT_TRUE(timeout_callback);
    }
}
