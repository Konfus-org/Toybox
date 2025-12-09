#include "tbx/app/app_message_coordinator.h"
#include "tbx/debugging/macros.h"
#include <exception>
#include <string>
#include <string_view>
#include <utility>

namespace tbx
{
    // ------------------------
    // Internal Helpers
    // ------------------------

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
                    const std::string& current = msg.result.get_report();
                    if (current.empty())
                    {
                        if (state == MessageState::Failed)
                            resolved = "Message processing failed.";
                        else if (state == MessageState::TimedOut)
                            resolved = "Message processing timed out.";
                        else
                            resolved = "Message was cancelled.";
                    }
                }

                msg.result.flag_failure(resolved);
                const std::string& text = msg.result.get_report();
                if (!text.empty())
                {
                    if (state == MessageState::Failed)
                    {
                        TBX_TRACE_ERROR(
                            "Message {} failed: {}",
                            to_string(msg.id).c_str(),
                            text.c_str());
                    }
                    else if (state == MessageState::TimedOut)
                    {
                        TBX_TRACE_WARNING(
                            "Message {} timed out: {}",
                            to_string(msg.id).c_str(),
                            text.c_str());
                    }
                }
                break;
            }
            case MessageState::InProgress:
            {
                if (!message.empty())
                    msg.result.flag_failure(message);
                else
                    msg.result.flag_failure(std::string());
                break;
            }
            default:
            {
                TBX_ASSERT(false, "Cannot process, unknown message state!");
                break;
            }
        }
    }

    static void dispatch_state_callbacks(const Message& msg, const MessageState state)
    {
        switch (state)
        {
            case MessageState::Handled:
            {
                if (msg.callbacks.on_handled)
                    msg.callbacks.on_handled(msg);
                break;
            }
            case MessageState::Cancelled:
            {
                if (msg.callbacks.on_cancelled)
                    msg.callbacks.on_cancelled(msg);
                break;
            }
            case MessageState::Failed:
            {
                if (msg.callbacks.on_failure)
                    msg.callbacks.on_failure(msg);
                break;
            }
            case MessageState::TimedOut:
            {
                if (msg.callbacks.on_timeout)
                    msg.callbacks.on_timeout(msg);
                break;
            }
            case MessageState::Processed:
            case MessageState::InProgress:
            {
                break;
            }
            default:
            {
                TBX_ASSERT(false, "Cannot process, unknown message state!");
                break;
            }
        }

        if (msg.callbacks.on_processed)
            msg.callbacks.on_processed(msg);
    }

    static void apply_state(Message& msg, MessageState state, const std::string& reason)
    {
        if (msg.state == state && reason.empty())
            return;

        msg.state = state;
        update_result_for_state(msg, state, reason);
        dispatch_state_callbacks(msg, state);
    }

    static void handle_state_change(const Message& msg, const MessageState previous_state)
    {
        if (msg.state == previous_state)
            return;

        update_result_for_state(msg, msg.state, std::string());
        dispatch_state_callbacks(msg, msg.state);
    }

    static bool cancel_if_requested(Message& msg, const std::string_view reason = "")
    {
        if (!msg.cancellation_token || !msg.cancellation_token.is_cancelled())
            return false;

        if (msg.state == MessageState::Cancelled)
            return true;

        std::string resolved =
            reason.empty() ? std::string("Message was cancelled.") : std::string(reason);
        apply_state(msg, MessageState::Cancelled, resolved);

        return true;
    }

    // ----------------------
    // AppMessageCoordinator
    // ----------------------

    AppMessageCoordinator::AppMessageCoordinator() = default;
    AppMessageCoordinator::~AppMessageCoordinator()
    {
        clear();
    }

    Uuid AppMessageCoordinator::add_handler(MessageHandler handler)
    {
        std::lock_guard lock(_handlers_mutex);
        Uuid id = Uuid::generate();
        _handlers.emplace_back(id, std::move(handler));
        return id;
    }

    void AppMessageCoordinator::remove_handler(const Uuid& token)
    {
        std::lock_guard lock(_handlers_mutex);
        std::vector<std::pair<Uuid, MessageHandler>> next;
        next.reserve(_handlers.size());
        for (auto& entry : _handlers)
        {
            if (entry.first != token)
            {
                next.emplace_back(std::move(entry));
            }
        }
        _handlers.swap(next);
    }

    void AppMessageCoordinator::clear()
    {
        // First process any pending messages
        process();

        // Then clear handlers and pending messages
        std::lock_guard locka(_handlers_mutex);
        _handlers.clear();
        std::lock_guard lockb(_queue_mutex);
        _pending.clear();
    }

    void AppMessageCoordinator::dispatch(Message& msg) const
    {
        try
        {
            std::vector<std::pair<Uuid, MessageHandler>> handlers_snapshot;
            {
                std::lock_guard lock(_handlers_mutex);
                handlers_snapshot = _handlers;
            }

            if (cancel_if_requested(msg))
                return;

            MessageState previous_state = msg.state;
            for (const auto& [id, handler] : handlers_snapshot)
            {
                if (!handler)
                {
                    TBX_ASSERT(
                        false,
                        "Message is registered as having a handler, but hanlder was null! This is "
                        "a "
                        "memory leak!");
                    continue;
                }

                handler(msg);

                if (msg.state != previous_state)
                {
                    handle_state_change(msg, previous_state);
                    previous_state = msg.state;
                }

                if (msg.state == MessageState::Handled)
                    break;
                if (msg.state == MessageState::Cancelled)
                    return;
                if (cancel_if_requested(msg))
                    return;
            }

            if (msg.state == MessageState::InProgress)
            {
                if (msg.require_handling)
                {
                    apply_state(
                        msg,
                        MessageState::Failed,
                        "Message required handling but no handlers completed it.");
                }
                else
                    apply_state(msg, MessageState::Processed, std::string());
            }

            TBX_ASSERT(msg.result.succeeded(), "Message failed!");
        }
        catch (const std::exception& ex)
        {
            apply_state(msg, MessageState::Failed, ex.what());
            TBX_ASSERT(false, "Exception during message dispatch: %s", ex.what());
        }
        catch (...)
        {
            apply_state(msg, MessageState::Failed, "Unknown exception during message dispatch.");
            TBX_ASSERT(false, "Unknown exception during message dispatch.");
        }
    }

    Result AppMessageCoordinator::send_impl(Message& msg) const
    {
        dispatch(msg);
        return msg.result;
    }

    std::future<Result> AppMessageCoordinator::post_impl(Scope<Message> msg) const
    {
        auto completion = Ref<std::promise<Result>>(std::promise<Result>());
        auto future = completion->get_future();

        try
        {
            auto lock = std::lock_guard<std::mutex>(_queue_mutex);
            _pending.emplace_back(std::move(msg), completion);
        }
        catch (const std::exception& ex)
        {
            apply_state(*msg, MessageState::Failed, ex.what());
            completion->set_value(msg->result);
        }
        catch (...)
        {
            apply_state(*msg, MessageState::Failed, "Unknown exception while queuing message.");
            completion->set_value(msg->result);
        }

        return future;
    }

    void AppMessageCoordinator::process()
    {
        std::vector<QueuedMessage> processing;
        {
            std::lock_guard<std::mutex> lock(_queue_mutex);
            processing.swap(_pending);
        }

        for (auto& entry : processing)
        {
            try
            {
                dispatch(*entry.message);
            }
            catch (const std::exception& ex)
            {
                apply_state(*entry.message, MessageState::Failed, ex.what());
            }
            catch (...)
            {
                apply_state(
                    *entry.message,
                    MessageState::Failed,
                    "Unknown exception during message dispatch.");
            }

            entry.completion->set_value(entry.message->result);
        }
    }
}
