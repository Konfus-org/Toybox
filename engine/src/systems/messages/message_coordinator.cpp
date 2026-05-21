#include "tbx/systems/messages/message_coordinator.h"
#include "tbx/systems/debugging/macros.h"
#include "systems/messages/internal/message_coordinator_internal.h"
#include <algorithm>
#include <exception>
#include <mutex>
#include <string>
#include <utility>
namespace tbx
{
    // ----------------------
    // MessageCoordinator
    // ----------------------

    MessageCoordinator::MessageCoordinator()
        : _handlers_snapshot(std::make_shared<const std::vector<RegisteredMessageHandler>>())
    {
    }

    MessageCoordinator::~MessageCoordinator() noexcept
    {
        flush();
        clear_handlers();
    }

    Uuid MessageCoordinator::register_handler(MessageHandler handler)
    {
        Uuid id = Uuid::generate();
        std::lock_guard<std::mutex> lock(_handlers_write_mutex);

        auto current = get_handlers_snapshot();
        auto next = std::make_shared<std::vector<RegisteredMessageHandler>>(*current);
        next->push_back(
            RegisteredMessageHandler {
                .id = id,
                .handler = std::make_shared<MessageHandler>(std::move(handler)),
            });
        _handlers_snapshot.store(next, std::memory_order_release);

        return id;
    }

    void MessageCoordinator::deregister_handler(const Uuid& token)
    {
        std::lock_guard<std::mutex> lock(_handlers_write_mutex);

        auto current = get_handlers_snapshot();
        auto next = std::make_shared<std::vector<RegisteredMessageHandler>>(*current);
        std::erase_if(
            *next,
            [&](const RegisteredMessageHandler& entry)
            {
                return entry.id == token;
            });
        _handlers_snapshot.store(next, std::memory_order_release);
    }

    void MessageCoordinator::clear_handlers()
    {
        std::lock_guard<std::mutex> lock(_handlers_write_mutex);

        auto cleared = std::make_shared<const std::vector<RegisteredMessageHandler>>();
        _handlers_snapshot.store(cleared, std::memory_order_release);
    }

    std::shared_ptr<const std::vector<RegisteredMessageHandler>> MessageCoordinator::
        get_handlers_snapshot() const
    {
        return _handlers_snapshot.load(std::memory_order_acquire);
    }

    void MessageCoordinator::dispatch(Message& msg) const
    {
        try
        {
            auto handlers_snapshot = get_handlers_snapshot();

            if (internal::cancel_if_requested(msg))
                return;

            MessageState previous_state = msg.state;
            for (const auto& entry : *handlers_snapshot)
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
                    internal::handle_state_change(msg, previous_state);
                    previous_state = msg.state;
                }

                if (msg.state == MessageState::HANDLED)
                    break;
                if (msg.state == MessageState::CANCELLED)
                    return;
                if (msg.state == MessageState::ERROR)
                    return;
                if (internal::cancel_if_requested(msg))
                    return;
            }

            if (msg.state == MessageState::UN_HANDLED)
            {
                auto request = handle_message<RequestBase>(msg);
                if (!request.has_value())
                {
                    internal::apply_state(msg, MessageState::UN_HANDLED, std::string());
                    return;
                }

                switch (request->get().not_handled_behavior)
                {
                    case MessageNotHandledBehavior::DO_NOTHING:
                    {
                        internal::apply_state(msg, MessageState::UN_HANDLED, std::string());
                        break;
                    }
                    case MessageNotHandledBehavior::WARN:
                    {
                        TBX_TRACE_WARNING(
                            "Request was not handled (type: %s).",
                            typeid(msg).name());
                        internal::apply_state(
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
                        internal::apply_state(
                            msg,
                            MessageState::ERROR,
                            "Request required handling but was not handled by any handlers.");
                        break;
                    }
                    default:
                    {
                        TBX_ASSERT(false, "Unknown MessageNotHandledBehavior.");
                        internal::apply_state(
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
            internal::apply_state(msg, MessageState::ERROR, ex.what());
            TBX_ASSERT(false, "Exception during message dispatch: %s", ex.what());
        }
        catch (...)
        {
            internal::apply_state(
                msg,
                MessageState::ERROR,
                "Unknown exception during message dispatch.");
            TBX_ASSERT(false, "Unknown exception during message dispatch.");
        }
    }

    Result MessageCoordinator::send(Message& msg) const
    {
        dispatch(msg);
        return msg.result;
    }

    std::shared_future<Result> MessageCoordinator::post(std::unique_ptr<Message> msg) const
    {
        auto completion_state = std::make_shared<std::promise<Result>>();
        auto future = completion_state->get_future().share();
        auto on_processed = msg->callbacks.on_processed;

        msg->callbacks.on_processed =
            [completion_state = std::move(completion_state),
             on_processed = std::move(on_processed)](const Message& processed)
        {
            if (on_processed)
            {
                on_processed(processed);
            }

            completion_state->set_value(processed.result);
        };

        {
            std::lock_guard<std::mutex> lock(_pending_mutex);
            _pending.emplace_back(QueuedMessage {std::move(msg)});
        }

        return future;
    }

    void MessageCoordinator::flush()
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
                internal::apply_state(*entry.message, MessageState::ERROR, ex.what());
            }
            catch (...)
            {
                internal::apply_state(
                    *entry.message,
                    MessageState::ERROR,
                    "Unknown exception during message dispatch.");
            }
        }
    }
}
