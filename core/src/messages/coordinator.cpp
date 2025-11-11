#include "tbx/messages/coordinator.h"
#include "tbx/debug/macros.h"
#include "tbx/tsl/smart_pointers.h"
#include <chrono>
#include <exception>
#include <string>
#include <string_view>

namespace tbx
{
    // ------------------------
    // Internal Helpers
    // ------------------------

    static void reset_message(Message& msg)
    {
        msg.state = MessageState::InProgress;
        msg.result = Result();
    }

    static void synchronize_original(QueuedMessage& queued)
    {
        if (!queued.original || !queued.message)
        {
            return;
        }

        queued.original->state = queued.message->state;
        queued.original->payload = queued.message->payload;
        queued.original->result = queued.message->result;
    }

    static void update_result_for_state(
        const Message& msg,
        const MessageState state,
        const std::string& message)
    {
        switch (state)
        {
            case MessageState::Handled:
            case MessageState::Processed:
            {
                msg.result.flag_success(message);
                break;
            }
            case MessageState::Cancelled:
            case MessageState::Failed:
            case MessageState::TimedOut:
            {
                std::string resolved = message;
                if (resolved.empty())
                {
                    const String& current = msg.result.get_report();
                    if (current.is_empty())
                    {
                        if (state == MessageState::Failed)
                        {
                            resolved = "Message processing failed.";
                        }
                        else if (state == MessageState::TimedOut)
                        {
                            resolved = "Message processing timed out.";
                        }
                        else
                        {
                            resolved = "Message was cancelled.";
                        }
                    }
                }

                msg.result.flag_failure(resolved);
                const String& text = msg.result.get_report();
                if (!text.is_empty())
                {
                    if (state == MessageState::Failed)
                    {
                        TBX_TRACE_ERROR(
                            "Message {} failed: {}",
                            to_string(msg.id).c_str(),
                            text.get_raw());
                    }
                    else if (state == MessageState::TimedOut)
                    {
                        TBX_TRACE_WARNING(
                            "Message {} timed out: {}",
                            to_string(msg.id).c_str(),
                            text.get_raw());
                    }
                }
                break;
            }
            case MessageState::InProgress:
            {
                if (!message.empty())
                {
                    msg.result.flag_failure(message);
                }
                else
                {
                    msg.result.flag_failure(std::string());
                }
                break;
            }
            default:
                TBX_ASSERT(false, "Cannot process, unknown message state!");
                break;
        }
    }

    static void dispatch_state_callbacks(const Message& msg, const MessageState state)
    {
        switch (state)
        {
            case MessageState::Handled:
                if (msg.callbacks.on_handled)
                {
                    msg.callbacks.on_handled(msg);
                }
                break;
            case MessageState::Cancelled:
                if (msg.callbacks.on_cancelled)
                {
                    msg.callbacks.on_cancelled(msg);
                }
                break;
            case MessageState::Failed:
                if (msg.callbacks.on_failure)
                {
                    msg.callbacks.on_failure(msg);
                }
                break;
            case MessageState::TimedOut:
                if (msg.callbacks.on_timeout)
                {
                    msg.callbacks.on_timeout(msg);
                }
                break;
            case MessageState::Processed:
            case MessageState::InProgress:
                break;
            default:
                TBX_ASSERT(false, "Cannot process, unknown message state!");
                break;
        }

        if (msg.callbacks.on_processed)
        {
            msg.callbacks.on_processed(msg);
        }
    }

    static void apply_state(Message& msg, MessageState state, const std::string& reason)
    {
        if (msg.state == state && reason.empty())
        {
            return;
        }

        msg.state = state;
        update_result_for_state(msg, state, reason);
        dispatch_state_callbacks(msg, state);
    }

    static void handle_state_change(const Message& msg, const MessageState previous_state)
    {
        if (msg.state == previous_state)
        {
            return;
        }

        update_result_for_state(msg, msg.state, std::string());
        dispatch_state_callbacks(msg, msg.state);
    }

    static bool cancel_if_requested(Message& msg, const std::string_view reason = "")
    {
        if (!msg.cancellation_token || !msg.cancellation_token.is_cancelled())
        {
            return false;
        }

        if (msg.state == MessageState::Cancelled)
        {
            return true;
        }

        std::string resolved =
            reason.empty() ? std::string("Message was cancelled.") : std::string(reason);
        apply_state(msg, MessageState::Cancelled, resolved);

        return true;
    }

    // -------------------
    // MessageCoordinator
    // -------------------

    MessageCoordinator::MessageCoordinator() = default;
    MessageCoordinator::~MessageCoordinator() = default;

    Uuid MessageCoordinator::add_handler(MessageHandler handler)
    {
        std::lock_guard lock(_handlers_mutex);
        Uuid id = Uuid::generate();
        _handlers.emplace_back(id, std::move(handler));
        return id;
    }

    void MessageCoordinator::remove_handler(const Uuid& token)
    {
        std::lock_guard lock(_handlers_mutex);
        List<std::pair<Uuid, MessageHandler>> next;
        next.reserve(_handlers.get_count());
        for (auto& entry : _handlers)
        {
            if (entry.first != token)
            {
                next.emplace_back(std::move(entry));
            }
        }
        _handlers.swap(next);
    }

    void MessageCoordinator::clear()
    {
        std::lock_guard locka(_handlers_mutex);
        _handlers.clear();

        std::lock_guard lockb(_queue_mutex);
        _pending.clear();
    }

    void MessageCoordinator::dispatch(Message& msg, std::chrono::steady_clock::time_point deadline)
        const
    {
        List<std::pair<Uuid, MessageHandler>> handlers_snapshot;
        {
            std::lock_guard lock(_handlers_mutex);
            handlers_snapshot = _handlers;
        }

        if (cancel_if_requested(msg))
        {
            return;
        }

        if (msg.timeout)
        {
            auto duration = msg.timeout.to_duration();
            if (duration <= std::chrono::steady_clock::duration::zero())
            {
                apply_state(
                    msg,
                    MessageState::TimedOut,
                    "Message timed out before dispatch began.");
                return;
            }
            deadline = std::chrono::steady_clock::now() + duration;
        }

        bool handler_invoked = false;
        MessageState previous_state = msg.state;
        for (const auto& entry : handlers_snapshot)
        {
            if (!entry.second)
            {
                continue;
            }

            if (msg.timeout && std::chrono::steady_clock::now() >= deadline)
            {
                apply_state(msg, MessageState::TimedOut, "Message timed out during dispatch.");
                return;
            }

            handler_invoked = true;
            entry.second(msg);

            if (msg.state != previous_state)
            {
                handle_state_change(msg, previous_state);
                previous_state = msg.state;
            }

            if (msg.state == MessageState::Handled)
            {
                break;
            }

            if (msg.state == MessageState::Cancelled)
            {
                return;
            }

            if (cancel_if_requested(msg))
            {
                return;
            }

            if (msg.timeout && std::chrono::steady_clock::now() >= deadline)
            {
                apply_state(msg, MessageState::TimedOut, "Message timed out during dispatch.");
                return;
            }
        }

        if (msg.state == MessageState::InProgress)
        {
            if (msg.timeout && std::chrono::steady_clock::now() >= deadline)
            {
                apply_state(msg, MessageState::TimedOut, "Message timed out during dispatch.");
            }
            else if (handler_invoked)
            {
                apply_state(
                    msg,
                    MessageState::Failed,
                    "Message handlers executed but did not update the message state.");
            }
            else
            {
                apply_state(msg, MessageState::Processed, std::string());
            }
        }
    }

    Result MessageCoordinator::send(Message& msg) const
    {
        reset_message(msg);
        if (msg.delay_in_ticks || msg.delay_in_seconds)
        {
            std::string reason = "send() does not support delayed delivery.";
            apply_state(msg, MessageState::Failed, reason);
            TBX_ASSERT(false, "Message delays are incompatible with send().");
            return msg.result;
        }

        if (cancel_if_requested(msg))
        {
            return msg.result;
        }

        try
        {
            dispatch(msg);
        }
        catch (const std::exception& ex)
        {
            apply_state(msg, MessageState::Failed, ex.what());
        }
        catch (...)
        {
            apply_state(msg, MessageState::Failed, "Unknown exception during message dispatch.");
        }
        return msg.result;
    }

    Result MessageCoordinator::post(const Message& msg)
    {
        struct Copy final : Message
        {
            explicit Copy(const Message& m)
            {
                *static_cast<Message*>(this) = m;
            }
        };

        auto& mutable_msg = const_cast<Message&>(msg);

        if (msg.delay_in_ticks && msg.delay_in_seconds)
        {
            std::string reason = "Message cannot specify both tick and time delays.";
            apply_state(mutable_msg, MessageState::Failed, reason);
            TBX_ASSERT(false, "Message cannot specify both tick and time delays.");
            return mutable_msg.result;
        }

        Result result;
        try
        {
            QueuedMessage queued;
            queued.message = Scope<Message>(new Copy(msg));
            if (queued.message)
            {
                reset_message(*queued.message);
                queued.result = queued.message->result;
                result = queued.result;
                mutable_msg.result = queued.result;
                mutable_msg.state = MessageState::InProgress;
            }

            queued.original = &mutable_msg;

            const auto now = std::chrono::steady_clock::now();
            if (msg.delay_in_ticks)
            {
                queued.timer = Timer::for_ticks(msg.delay_in_ticks);
            }
            else if (msg.delay_in_seconds)
            {
                queued.timer = Timer::for_time_span(msg.delay_in_seconds, now);
            }
            else
            {
                queued.timer.reset();
            }

            if (msg.timeout && !msg.timeout.is_zero())
            {
                queued.timeout_deadline = now + msg.timeout.to_duration();
            }

            std::lock_guard<std::mutex> lock(_queue_mutex);
            _pending.emplace_back(std::move(queued));
        }
        catch (const std::exception& ex)
        {
            apply_state(mutable_msg, MessageState::Failed, ex.what());
            return mutable_msg.result;
        }
        catch (...)
        {
            apply_state(
                mutable_msg,
                MessageState::Failed,
                "Unknown exception while queuing message.");
            return mutable_msg.result;
        }

        return result;
    }

    void MessageCoordinator::process()
    {
        const auto now = std::chrono::steady_clock::now();

        List<QueuedMessage> processing;
        {
            std::lock_guard<std::mutex> lock(_queue_mutex);
            processing.swap(_pending);
        }

        for (auto& entry : processing)
        {
            if (!entry.message)
            {
                continue;
            }

            if (cancel_if_requested(*entry.message))
            {
                synchronize_original(entry);
                continue;
            }

            if (entry.message->timeout && now >= entry.timeout_deadline)
            {
                apply_state(
                    *entry.message,
                    MessageState::TimedOut,
                    "Message timed out before delivery.");
                synchronize_original(entry);
                continue;
            }

            if (entry.timer.tick())
            {
                std::lock_guard lock(_queue_mutex);
                _pending.emplace_back(std::move(entry));
                continue;
            }

            if (!entry.timer.is_time_up(now))
            {
                std::lock_guard lock(_queue_mutex);
                _pending.emplace_back(std::move(entry));
                continue;
            }

            try
            {
                dispatch(*entry.message, entry.timeout_deadline);
                synchronize_original(entry);
            }
            catch (const std::exception& ex)
            {
                apply_state(*entry.message, MessageState::Failed, ex.what());
                synchronize_original(entry);
            }
            catch (...)
            {
                apply_state(
                    *entry.message,
                    MessageState::Failed,
                    "Unknown exception during message dispatch.");
                synchronize_original(entry);
            }
        }
    }
}
