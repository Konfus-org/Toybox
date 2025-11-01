#include "pch.h"
#include "tbx/messages/coordinator.h"
#include "tbx/messages/message.h"
#include "tbx/messages/handler.h"
#include "tbx/state/cancellation_token.h"
#include <atomic>
#include <chrono>
#include <stdexcept>
#include <string>
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
            const_cast<::tbx::Message&>(msg).is_handled = true;
        });
        d.add_handler([&](const ::tbx::Message& msg)
        {
            count.fetch_add(1);
        });

        TestMessage msg;
        msg.value = 42;
        bool handled_callback = false;
        bool processed_callback = false;
        msg.on_handled = [&](const ::tbx::Message&)
        {
            handled_callback = true;
        };
        msg.on_processed = [&](const ::tbx::Message&)
        {
            processed_callback = true;
        };

        auto result = d.send(msg);
        EXPECT_EQ(count.load(), 1);
        EXPECT_EQ(result.get_status(), ::tbx::MessageStatus::Handled);
        EXPECT_TRUE(handled_callback);
        EXPECT_TRUE(processed_callback);
    }

    TEST(dispatcher_send_no_handlers, returns_processed_without_callbacks)
    {
        ::tbx::MessageCoordinator d;

        ::tbx::Message msg;
        bool processed_callback = false;
        msg.on_processed = [&](const ::tbx::Message&)
        {
            processed_callback = true;
        };

        auto result = d.send(msg);

        EXPECT_EQ(result.get_status(), ::tbx::MessageStatus::Processed);
        EXPECT_TRUE(processed_callback);
        EXPECT_TRUE(result.get_message().empty());
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
        msg.on_failure = [&](const ::tbx::Message&)
        {
            failure_callback = true;
        };
        msg.on_processed = [&](const ::tbx::Message&)
        {
            processed_callback = true;
        };

        auto result = d.send(msg);

        EXPECT_EQ(count.load(), 1);
        EXPECT_EQ(result.get_status(), ::tbx::MessageStatus::Failed);
        EXPECT_FALSE(result.get_message().empty());
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
        msg.on_failure = [&](const ::tbx::Message&)
        {
            failure_callback = true;
        };
        msg.on_processed = [&](const ::tbx::Message&)
        {
            processed_callback = true;
        };

        auto result = d.send(msg);

        EXPECT_EQ(result.get_status(), ::tbx::MessageStatus::Failed);
        EXPECT_TRUE(failure_callback);
        EXPECT_TRUE(processed_callback);
        EXPECT_FALSE(result.get_message().empty());
    }

    TEST(dispatcher_send_with_delay, returns_failure_for_incompatible_config)
    {
        ::tbx::MessageCoordinator d;

        ::tbx::Message msg;
        msg.delay_ticks = 1;
#ifdef TBX_ASSERTS_ENABLED
        EXPECT_DEBUG_DEATH(d.send(msg), ".*");
#else
        bool failure_callback = false;
        bool processed_callback = false;
        msg.on_failure = [&](const ::tbx::Message&)
        {
            failure_callback = true;
        };
        msg.on_processed = [&](const ::tbx::Message&)
        {
            processed_callback = true;
        };

        auto result = d.send(msg);

        EXPECT_EQ(result.get_status(), ::tbx::MessageStatus::Failed);
        EXPECT_TRUE(failure_callback);
        EXPECT_TRUE(processed_callback);
        EXPECT_FALSE(result.get_message().empty());
#endif
    }

    TEST(dispatcher_post, processes_on_next_update)
    {
        ::tbx::MessageCoordinator d;
        std::atomic<int> count{0};

        d.add_handler([&](const ::tbx::Message& msg)
        {
            count.fetch_add(1);
            const_cast<::tbx::Message&>(msg).is_handled = true;
        });

        ::tbx::Message msg;
        auto result = d.post(msg);

        // Not processed yet
        EXPECT_EQ(count.load(), 0);
        EXPECT_EQ(result.get_status(), ::tbx::MessageStatus::InProgress);

        d.process();
        EXPECT_EQ(count.load(), 1);
        EXPECT_EQ(result.get_status(), ::tbx::MessageStatus::Handled);
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
        EXPECT_EQ(result.get_status(), ::tbx::MessageStatus::Failed);
        EXPECT_FALSE(result.get_message().empty());
        (void)keep_id; // silence unused warning
    }

    TEST(dispatcher_post_tick_delay, delays_processing_by_ticks)
    {
        ::tbx::MessageCoordinator d;
        std::atomic<int> count{0};

        d.add_handler([&](const ::tbx::Message& msg)
        {
            count.fetch_add(1);
            const_cast<::tbx::Message&>(msg).is_handled = true;
        });

        ::tbx::Message msg;
        msg.delay_ticks = 1;
        auto result = d.post(msg);

        EXPECT_EQ(result.get_status(), ::tbx::MessageStatus::InProgress);
        d.process();
        EXPECT_EQ(result.get_status(), ::tbx::MessageStatus::InProgress);
        EXPECT_EQ(count.load(), 0);

        d.process();
        EXPECT_EQ(result.get_status(), ::tbx::MessageStatus::Handled);
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
            const_cast<::tbx::Message&>(msg).is_handled = true;
        });

        ::tbx::Message msg;
        msg.delay_ticks = 1;
        ::tbx::TimeSpan span;
        span.milliseconds = 5;
        msg.delay_time = span;
#ifdef TBX_ASSERTS_ENABLED
        EXPECT_DEBUG_DEATH(d.post(msg), ".*");
#else
        bool failure_callback = false;
        bool processed_callback = false;
        msg.on_failure = [&](const ::tbx::Message&)
        {
            failure_callback = true;
        };
        msg.on_processed = [&](const ::tbx::Message&)
        {
            processed_callback = true;
        };

        auto result = d.post(msg);

        EXPECT_EQ(result.get_status(), ::tbx::MessageStatus::Failed);
        EXPECT_TRUE(failure_callback);
        EXPECT_TRUE(processed_callback);
        EXPECT_FALSE(result.get_message().empty());
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
        msg.on_failure = [&](const ::tbx::Message&)
        {
            failure_callback = true;
        };
        msg.on_processed = [&](const ::tbx::Message&)
        {
            processed_callback = true;
        };
        auto result = d.post(msg);

        d.process();

        EXPECT_EQ(result.get_status(), ::tbx::MessageStatus::Failed);
        EXPECT_TRUE(failure_callback);
        EXPECT_TRUE(processed_callback);
        EXPECT_FALSE(result.get_message().empty());
    }

    TEST(dispatcher_post_time_delay, delays_processing_by_timespan)
    {
        using namespace std::chrono_literals;

        ::tbx::MessageCoordinator d;
        std::atomic<int> count{0};

        d.add_handler([&](const ::tbx::Message& msg)
        {
            count.fetch_add(1);
            const_cast<::tbx::Message&>(msg).is_handled = true;
        });

        ::tbx::Message msg;
        ::tbx::TimeSpan delay;
        delay.milliseconds = 5;
        msg.delay_time = delay;
        auto result = d.post(msg);

        EXPECT_EQ(result.get_status(), ::tbx::MessageStatus::InProgress);
        d.process();
        EXPECT_EQ(result.get_status(), ::tbx::MessageStatus::InProgress);
        EXPECT_EQ(count.load(), 0);

        std::this_thread::sleep_for(6ms);

        d.process();
        EXPECT_EQ(result.get_status(), ::tbx::MessageStatus::Handled);
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
        auto token = source.token();

        ::tbx::Message msg;
        msg.cancellation_token = token;
        bool cancelled_callback = false;
        bool processed_callback = false;
        msg.on_cancelled = [&](const ::tbx::Message&)
        {
            cancelled_callback = true;
        };
        msg.on_processed = [&](const ::tbx::Message&)
        {
            processed_callback = true;
        };
        auto result = d.post(msg);

        source.cancel();
        d.process();

        EXPECT_EQ(result.get_status(), ::tbx::MessageStatus::Cancelled);
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
        auto token = source.token();
        source.cancel();

        ::tbx::Message msg;
        msg.cancellation_token = token;
        bool cancelled_callback = false;
        msg.on_cancelled = [&](const ::tbx::Message&)
        {
            cancelled_callback = true;
        };

        auto result = d.send(msg);

        EXPECT_EQ(result.get_status(), ::tbx::MessageStatus::Cancelled);
        EXPECT_TRUE(cancelled_callback);
        EXPECT_EQ(count.load(), 0);
    }

    TEST(dispatcher_result_value, handler_populates_result_payload)
    {
        ::tbx::MessageCoordinator d;

        d.add_handler([](const ::tbx::Message& message)
        {
            auto& mutable_msg = const_cast<::tbx::Message&>(message);
            mutable_msg.is_handled = true;
            auto* result_ptr = mutable_msg.get_result();
            ASSERT_NE(result_ptr, nullptr);
            result_ptr->set_payload<int>(123);
        });

        ::tbx::Message msg;
        auto result = d.send(msg);

        EXPECT_EQ(result.get_status(), ::tbx::MessageStatus::Handled);
        EXPECT_TRUE(result.has_payload());
        auto value = result.try_get_payload<int>();
        ASSERT_NE(value, nullptr);
        EXPECT_EQ(*value, 123);
        EXPECT_EQ(result.try_get_payload<float>(), nullptr);
    }

    TEST(dispatcher_post_result_value, queued_handler_updates_payload)
    {
        ::tbx::MessageCoordinator d;

        d.add_handler([](const ::tbx::Message& message)
        {
            auto& mutable_msg = const_cast<::tbx::Message&>(message);
            mutable_msg.is_handled = true;
            auto* result_ptr = mutable_msg.get_result();
            ASSERT_NE(result_ptr, nullptr);
            result_ptr->set_payload<std::string>("ready");
        });

        ::tbx::Message msg;
        auto result = d.post(msg);

        EXPECT_EQ(result.get_status(), ::tbx::MessageStatus::InProgress);
        EXPECT_FALSE(result.has_payload());

        d.process();

        EXPECT_EQ(result.get_status(), ::tbx::MessageStatus::Handled);
        EXPECT_TRUE(result.has_payload());
        const auto* final_value = result.try_get_payload<std::string>();
        ASSERT_NE(final_value, nullptr);
        EXPECT_EQ(*final_value, "ready");
    }
}
