#include "tbx/app/app_message_coordinator.h"
#include "tbx/common/string.h"
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
        const String& message)
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
                String resolved = message;
                if (resolved.empty())
                {
                    const String& current = msg.result.get_report();
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
                const String& text = msg.result.get_report();
                if (!text.empty())
                {
                    const String message_id(msg.id);
                    if (state == MessageState::Failed)
                    {
                        TBX_TRACE_ERROR(
                            "Message {} failed: {}", message_id.std_str(), text);
                    }
                    else if (state == MessageState::TimedOut)
                    {
                        TBX_TRACE_WARNING(
                            "Message {} timed out: {}", message_id.std_str(), text);
                    }
                }
                break;
            }
            case MessageState::InProgress:
            {
                if (!message.empty())
                    msg.result.flag_failure(message);
                else
                    msg.result.flag_failure(String());
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

    static void apply_state(Message& msg, MessageState state, const String& reason)
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

        update_result_for_state(msg, msg.state, String());
        dispatch_state_callbacks(msg, msg.state);
    }

    static bool cancel_if_requested(Message& msg, const String& reason = String())
    {
        if (!msg.cancellation_token || !msg.cancellation_token.is_cancelled())
            return false;

        if (msg.state == MessageState::Cancelled)
            return true;

        String resolved = reason.empty() ? String("Message was cancelled.") : reason;
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
        auto lock = _handlers.lock();
        Uuid id = Uuid::generate();
        lock.get().emplace(id, std::move(handler));
        return id;
    }

    void AppMessageCoordinator::remove_handler(const Uuid& token)
    {
        auto lock = _handlers.lock();
        auto& handlers = lock.get();
        List<Pair<Uuid, MessageHandler>> next;
        next.reserve(handlers.size());
        for (auto& entry : handlers)
        {
            if (entry.first != token)
            {
                next.push_back(std::move(entry));
            }
        }
        handlers.swap(next);
    }

    void AppMessageCoordinator::clear()
    {
        // First process any pending messages
        process();

        // Then clear handlers and pending messages
        {
            auto lock = _handlers.lock();
            lock.get().clear();
        }
        {
            auto lock = _pending.lock();
            lock.get().clear();
        }
    }

    void AppMessageCoordinator::dispatch(Message& msg) const
    {
        try
        {
            List<Pair<Uuid, MessageHandler>> handlers_snapshot;
            {
                auto lock = _handlers.lock();
                handlers_snapshot = lock.get();
            }

            if (cancel_if_requested(msg))
                return;

            MessageState previous_state = msg.state;
            for (const auto& entry : handlers_snapshot)
            {
                if (!entry.second)
                {
                    TBX_ASSERT(
                        false,
                        "Message is registered as having a handler, but hanlder was null! This is "
                        "a "
                        "memory leak!");
                    continue;
                }

                entry.second(msg);

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
                    apply_state(msg, MessageState::Processed, String());
            }
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

    Promise<Result> AppMessageCoordinator::post_impl(Scope<Message> msg) const
    {
    Promise<Result> promise;
    Promise<Result> future = promise;

    {
        auto lock = _pending.lock();
        lock.get().emplace(std::move(msg), std::move(promise));
    }

    return future;
    }

    void AppMessageCoordinator::process()
    {
        List<QueuedMessage> processing;
        {
            auto lock = _pending.lock();
            processing.swap(lock.get());
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

            if (entry.completion_state)
            {
                entry.completion_state.fulfill(entry.message->result);
            }
        }
    }
}
