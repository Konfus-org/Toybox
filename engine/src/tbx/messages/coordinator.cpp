#include "tbx/messages/coordinator.h"
#include "tbx/memory/smart_pointers.h"
#include <chrono>
#include <exception>
#include <string>

namespace tbx
{
    Uuid MessageCoordinator::add_handler(MessageHandler handler)
    {
        Uuid id = Uuid::generate();
        _handlers.emplace_back(id, std::move(handler));
        return id;
    }

    void MessageCoordinator::remove_handler(const Uuid& token)
    {
        std::vector<std::pair<Uuid, MessageHandler>> next;
        next.reserve(_handlers.size());
        for (auto& entry : _handlers)
        {
            if (!(entry.first == token))
            {
                next.emplace_back(std::move(entry));
            }
        }
        _handlers.swap(next);
    }

    void MessageCoordinator::clear()
    {
        _handlers.clear();
        _pending.clear();
        _processing.clear();
    }

    void MessageCoordinator::dispatch(Message& msg, const MessageConfiguration& config, MessageResult& result) const
    {
        MessageResult* previous_result = msg.result;
        msg.result = &result;
        struct ResultGuard
        {
            Message& target;
            MessageResult* previous;
            ~ResultGuard() { target.result = previous; }
        } guard{msg, previous_result};

        if (config.cancellation_token && config.cancellation_token.is_cancelled())
        {
            finalize_callbacks(msg, config, result, MessageStatus::Cancelled);
            return;
        }

        bool handler_invoked = false;
        for (const auto& entry : _handlers)
        {
            if (msg.is_handled)
                break;
            if (entry.second)
            {
                handler_invoked = true;
                entry.second(msg);
            }
        }

        MessageStatus status = MessageStatus::Processed;
        const std::string* failure_reason = nullptr;
        std::string reason_storage;
        if (msg.is_handled)
        {
            status = MessageStatus::Handled;
        }
        else if (handler_invoked)
        {
            status = MessageStatus::Failed;
            reason_storage = "Message handlers executed but did not mark the message as handled.";
            failure_reason = &reason_storage;
        }

        finalize_callbacks(msg, config, result, status, failure_reason);
    }

    void MessageCoordinator::finalize_callbacks(const Message& msg, const MessageConfiguration& config, MessageResult& result, MessageStatus status, const std::string* failure_reason) const
    {
        if (status == MessageStatus::Failed)
        {
            if (!result.is_failed())
            {
                if (failure_reason)
                {
                    result.set_failure(*failure_reason);
                }
                else
                {
                    result.set_failure("Message processing failed.");
                }
            }
            else if (failure_reason && result.why().empty())
            {
                result.set_status(MessageStatus::Failed, *failure_reason);
            }
            else
            {
                result.set_status(MessageStatus::Failed);
            }
        }
        else
        {
            result.set_status(status);
        }

        switch (status)
        {
        case MessageStatus::Handled:
            if (config.on_handled)
                config.on_handled(msg);
            break;
        case MessageStatus::Cancelled:
            if (config.on_cancelled)
                config.on_cancelled(msg);
            break;
        case MessageStatus::Failed:
            if (config.on_failure)
                config.on_failure(msg);
            break;
        case MessageStatus::Processed:
        case MessageStatus::InProgress:
            break;
        }

        if (config.on_processed)
            config.on_processed(msg);
    }

    MessageResult MessageCoordinator::send(const Message& msg, const MessageConfiguration& config) const
    {
        MessageResult result;
        try
        {
            Message& mutable_msg = const_cast<Message&>(msg);
            dispatch(mutable_msg, config, result);
        }
        catch (const std::exception& ex)
        {
            std::string reason = ex.what();
            result.set_failure(reason);
            finalize_callbacks(msg, config, result, MessageStatus::Failed, &reason);
        }
        catch (...)
        {
            std::string reason = "Unknown exception during message dispatch.";
            result.set_failure(reason);
            finalize_callbacks(msg, config, result, MessageStatus::Failed, &reason);
        }
        return result;
    }

    MessageResult MessageCoordinator::post(const Message& msg, const MessageConfiguration& config)
    {
        struct Copy final : Message
        {
            explicit Copy(const Message& m) { *static_cast<Message*>(this) = m; }
        };

        MessageResult result;

        try
        {
            QueuedMessage queued;
            queued.message = make_scope<Copy>(msg);
            if (queued.message)
            {
                queued.message->result = nullptr;
            }
            queued.config = config;
            queued.result = result;

            queued.timer.reset();
            if (config.delay_ticks)
            {
                queued.timer.set_ticks(*config.delay_ticks);
            }

            const auto now = std::chrono::steady_clock::now();
            if (config.delay_time)
            {
                queued.timer.set_time(*config.delay_time, now);
            }

            _pending.emplace_back(std::move(queued));
        }
        catch (const std::exception& ex)
        {
            std::string reason = ex.what();
            result.set_failure(reason);
            finalize_callbacks(msg, config, result, MessageStatus::Failed, &reason);
        }
        catch (...)
        {
            std::string reason = "Unknown exception while queuing message.";
            result.set_failure(reason);
            finalize_callbacks(msg, config, result, MessageStatus::Failed, &reason);
        }

        return result;
    }

    void MessageCoordinator::process()
    {
        const auto now = std::chrono::steady_clock::now();

        _processing.clear();
        _processing.swap(_pending);
        for (auto& entry : _processing)
        {
            if (!entry.message)
                continue;

            if (entry.config.cancellation_token && entry.config.cancellation_token.is_cancelled())
            {
                MessageResult* previous_result = entry.message->result;
                entry.message->result = &entry.result;
                finalize_callbacks(*entry.message, entry.config, entry.result, MessageStatus::Cancelled);
                entry.message->result = previous_result;
                continue;
            }

            if (entry.timer.tick())
            {
                _pending.emplace_back(std::move(entry));
                continue;
            }

            if (!entry.timer.is_ready(now))
            {
                _pending.emplace_back(std::move(entry));
                continue;
            }

            dispatch(*entry.message, entry.config, entry.result);
        }

        _processing.clear();
    }
}
