#include "tbx/app/app_message_coordinator.h"
#include "tbx/debugging/macros.h"
#include <exception>
#include <mutex>
#include <string>
#include <utility>

namespace tbx
{
    // ------------------------
    // Internal Helpers
    // ------------------------

    static void update_result_for_state(
        const Message& msg,
        const MessageState& state,
        const std::string& message)
    {
        switch (state)
        {
            case MessageState::HANDLED:
            case MessageState::UN_HANDLED:
            {
                msg.result.flag_success(message);
                break;
            }
            case MessageState::CANCELLED:
            {
                msg.result.flag_failure(
                    message.empty() ? std::string("Message was cancelled.") : message);
                break;
            }
            case MessageState::ERROR:
            {
                msg.result.flag_failure(
                    message.empty() ? std::string("Message processing failed.") : message);
                break;
            }
            default:
            {
                TBX_ASSERT(false, "Failed to process msg, error occured!");
                break;
            }
        }
    }

    static void dispatch_state_callbacks(const Message& msg, const MessageState& state)
    {
        switch (state)
        {
            case MessageState::CANCELLED:
            {
                if (msg.callbacks.on_cancelled)
                    msg.callbacks.on_cancelled(msg);
                break;
            }
            case MessageState::ERROR:
            {
                if (msg.callbacks.on_error)
                    msg.callbacks.on_error(msg);
                break;
            }
            case MessageState::HANDLED:
            case MessageState::UN_HANDLED:
                break;
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
        msg.state = state;
        update_result_for_state(msg, state, reason);
        dispatch_state_callbacks(msg, state);
    }

    static void handle_state_change(const Message& msg, const MessageState& previous_state)
    {
        if (msg.state == previous_state)
            return;

        update_result_for_state(msg, msg.state, std::string());
        dispatch_state_callbacks(msg, msg.state);
    }

    static bool cancel_if_requested(Message& msg, const std::string& reason = std::string())
    {
        if (!msg.cancellation_token || !msg.cancellation_token.is_cancelled())
            return false;

        if (msg.state == MessageState::CANCELLED)
            return true;

        std::string resolved = reason.empty() ? std::string("Message was cancelled.") : reason;
        apply_state(msg, MessageState::CANCELLED, resolved);

        return true;
    }

    // ----------------------
    // AppMessageCoordinator
    // ----------------------

    AppMessageCoordinator::AppMessageCoordinator() = default;
    AppMessageCoordinator::~AppMessageCoordinator() noexcept
    {
        flush();
        clear_handlers();
    }

    Uuid AppMessageCoordinator::register_handler(MessageHandler handler)
    {
        Uuid id = Uuid::generate();
        {
            std::lock_guard<std::mutex> lock(_handlers_mutex);
            _handlers.push_back(
                RegisteredMessageHandler {
                    .id = id,
                    .handler = std::make_shared<MessageHandler>(std::move(handler)),
                });
        }
        return id;
    }

    void AppMessageCoordinator::deregister_handler(const Uuid& token)
    {
        std::lock_guard<std::mutex> lock(_handlers_mutex);
        std::vector<RegisteredMessageHandler> next;
        next.reserve(_handlers.size());
        for (auto& entry : _handlers)
        {
            if (entry.id != token)
                next.push_back(std::move(entry));
        }
        _handlers.swap(next);
    }

    void AppMessageCoordinator::clear_handlers()
    {
        std::lock_guard<std::mutex> lock(_handlers_mutex);
        _handlers.clear();
    }

    void AppMessageCoordinator::dispatch(Message& msg) const
    {
        try
        {
            std::vector<RegisteredMessageHandler> handlers_snapshot;
            {
                std::lock_guard<std::mutex> lock(_handlers_mutex);
                handlers_snapshot = _handlers;
            }

            if (cancel_if_requested(msg))
                return;

            MessageState previous_state = msg.state;
            for (const auto& entry : handlers_snapshot)
            {
                if (!entry.handler || !(*entry.handler))
                {
                    TBX_ASSERT(
                        false,
                        "Message is registered as having a handler, but hanlder was null! This is "
                        "a "
                        "memory leak!");
                    continue;
                }

                (*entry.handler)(msg);

                if (msg.state != previous_state)
                {
                    handle_state_change(msg, previous_state);
                    previous_state = msg.state;
                }

                if (msg.state == MessageState::HANDLED)
                    break;
                if (msg.state == MessageState::CANCELLED)
                    return;
                if (cancel_if_requested(msg))
                    return;
            }

            if (msg.state == MessageState::UN_HANDLED)
            {
                auto* request = dynamic_cast<RequestBase*>(&msg);
                if (!request)
                {
                    apply_state(msg, MessageState::UN_HANDLED, std::string());
                    return;
                }

                switch (request->not_handled_behavior)
                {
                    case MessageNotHandledBehavior::DO_NOTHING:
                    {
                        apply_state(msg, MessageState::UN_HANDLED, std::string());
                        break;
                    }
                    case MessageNotHandledBehavior::WARN:
                    {
                        TBX_TRACE_WARNING(
                            "Request was not handled (type: %s).",
                            typeid(msg).name());
                        apply_state(
                            msg,
                            MessageState::ERROR,
                            "Request was not handled by any handlers.");
                        break;
                    }
                    case MessageNotHandledBehavior::ASSERT:
                    {
                        TBX_ASSERT(
                            false,
                            "Request required handling but was not handled (type: %s).",
                            typeid(msg).name());
                        apply_state(
                            msg,
                            MessageState::ERROR,
                            "Request required handling but was not handled by any handlers.");
                        break;
                    }
                    default:
                    {
                        TBX_ASSERT(false, "Unknown MessageNotHandledBehavior.");
                        apply_state(
                            msg,
                            MessageState::ERROR,
                            "Unknown request not-handled behavior.");
                        break;
                    }
                }
            }
        }
        catch (const std::exception& ex)
        {
            apply_state(msg, MessageState::ERROR, ex.what());
            TBX_ASSERT(false, "Exception during message dispatch: %s", ex.what());
        }
        catch (...)
        {
            apply_state(msg, MessageState::ERROR, "Unknown exception during message dispatch.");
            TBX_ASSERT(false, "Unknown exception during message dispatch.");
        }
    }

    Result AppMessageCoordinator::send(Message& msg) const
    {
        dispatch(msg);
        return msg.result;
    }

    std::shared_future<Result> AppMessageCoordinator::post(std::unique_ptr<Message> msg) const
    {
        std::promise<Result> promise;
        auto future = promise.get_future().share();

        {
            std::lock_guard<std::mutex> lock(_pending_mutex);
            _pending.emplace_back(QueuedMessage {std::move(msg), std::move(promise)});
        }

        return future;
    }

    void AppMessageCoordinator::flush()
    {
        std::vector<QueuedMessage> processing;
        {
            std::lock_guard<std::mutex> lock(_pending_mutex);
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
                apply_state(*entry.message, MessageState::ERROR, ex.what());
            }
            catch (...)
            {
                apply_state(
                    *entry.message,
                    MessageState::ERROR,
                    "Unknown exception during message dispatch.");
            }

            entry.completion_state.set_value(entry.message->result);
        }
    }
}
