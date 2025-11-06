#include "tbx/messages/coordinator.h"
#include "tbx/debug/log_macros.h"
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

    void MessageCoordinator::dispatch(Message& msg, Result& result) const
    {
        MessageResult* previous_result = msg.get_result();
        msg.set_result(result);
        struct ResultGuard
        {
            Message& target;
            MessageResult* previous;
            ~ResultGuard()
            {
                if (previous)
                {
                    target.set_result(*previous);
                }
                else
                {
                    target.clear_result();
                }
            }
        } guard{msg, previous_result};

        if (msg.cancellation_token && msg.cancellation_token.is_cancelled())
        {
            finalize_callbacks(msg, result, ResultStatus::Cancelled);
            return;
        }

        bool handler_invoked = false;
        for (const auto& entry : _handlers)
        {
            if (msg.is_handled)
            {
                break;
            }

            if (entry.second)
            {
                handler_invoked = true;
                entry.second(msg);
            }
        }

        ResultStatus status = ResultStatus::Processed;
        const std::string* failure_reason = nullptr;
        std::string reason_storage;
        if (msg.is_handled)
        {
            status = ResultStatus::Handled;
        }
        else if (handler_invoked)
        {
            status = ResultStatus::Failed;
            reason_storage = "Message handlers executed but did not mark the message as handled.";
            failure_reason = &reason_storage;
        }

        finalize_callbacks(msg, result, status, failure_reason);
    }

    void MessageCoordinator::finalize_callbacks(const Message& msg, Result& result, ResultStatus status, const std::string* failure_reason) const
    {
        if (status == ResultStatus::Failed)
        {
            if (result.get_status() != ResultStatus::Failed)
            {
                if (failure_reason)
                {
                    result.set_status(ResultStatus::Failed, *failure_reason);
                }
                else
                {
                    result.set_status(ResultStatus::Failed, "Message processing failed.");
                }
            }
            else if (failure_reason && result.get_message().empty())
            {
                result.set_status(ResultStatus::Failed, *failure_reason);
            }
            else
            {
                result.set_status(ResultStatus::Failed);
            }
        }
        else
        {
            result.set_status(status);
        }

        switch (status)
        {
            case ResultStatus::Handled:
            {
                if (msg.on_handled)
                {
                    msg.on_handled(msg);
                }
                break;
            }
            case ResultStatus::Cancelled:
            {
                if (msg.on_cancelled)
                {
                    msg.on_cancelled(msg);
                }
                break;
            }
            case ResultStatus::Failed:
            {
                if (msg.on_failure)
                {
                    msg.on_failure(msg);
                }
                break;
            }
            case ResultStatus::Processed:
            case ResultStatus::InProgress:
            {
                break;
            }
        }

        if (msg.on_processed)
        {
            msg.on_processed(msg);
        }
    }

    Result MessageCoordinator::send(const Message& msg) const
    {
        Result result;
        if (msg.has_delay())
        {
            std::string reason = "send() does not support delayed delivery.";
            result.set_status(ResultStatus::Failed, reason);
            finalize_callbacks(msg, result, ResultStatus::Failed, &reason);
            TBX_ASSERT(false, "Message delays are incompatible with send().");
            return result;
        }
        try
        {
            Message& mutable_msg = const_cast<Message&>(msg);
            dispatch(mutable_msg, result);
        }
        catch (const std::exception& ex)
        {
            std::string reason = ex.what();
            result.set_status(ResultStatus::Failed, reason);
            finalize_callbacks(msg, result, ResultStatus::Failed, &reason);
        }
        catch (...)
        {
            std::string reason = "Unknown exception during message dispatch.";
            result.set_status(ResultStatus::Failed, reason);
            finalize_callbacks(msg, result, ResultStatus::Failed, &reason);
        }
        return result;
    }

    Result MessageCoordinator::post(const Message& msg)
    {
        struct Copy final : Message
        {
            explicit Copy(const Message& m) { *static_cast<Message*>(this) = m; }
        };

        Result result;

        if (msg.delay_ticks && msg.delay_time)
        {
            std::string reason = "Message cannot specify both tick and time delays.";
            result.set_status(ResultStatus::Failed, reason);
            finalize_callbacks(msg, result, ResultStatus::Failed, &reason);
            TBX_ASSERT(false, "Message cannot specify both tick and time delays.");
            return result;
        }

        try
        {
            QueuedMessage queued;
            queued.message = make_scope<Copy>(msg);
            if (queued.message)
            {
                queued.message->clear_result();
            }
            queued.result = result;

            const auto now = std::chrono::steady_clock::now();
            if (msg.delay_ticks)
            {
                queued.timer = Timer::for_ticks(*msg.delay_ticks);
            }
            else if (msg.delay_time)
            {
                queued.timer = Timer::for_time_span(*msg.delay_time, now);
            }
            else
            {
                queued.timer.reset();
            }

            _pending.emplace_back(std::move(queued));
        }
        catch (const std::exception& ex)
        {
            std::string reason = ex.what();
            result.set_status(ResultStatus::Failed, reason);
            finalize_callbacks(msg, result, ResultStatus::Failed, &reason);
        }
        catch (...)
        {
            std::string reason = "Unknown exception while queuing message.";
            result.set_status(ResultStatus::Failed, reason);
            finalize_callbacks(msg, result, ResultStatus::Failed, &reason);
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

            if (entry.message->cancellation_token && entry.message->cancellation_token.is_cancelled())
            {
                MessageResult* previous_result = entry.message->get_result();
                entry.message->set_result(entry.result);
                finalize_callbacks(*entry.message, entry.result, ResultStatus::Cancelled);
                if (previous_result)
                {
                    entry.message->set_result(*previous_result);
                }
                else
                {
                    entry.message->clear_result();
                }
                continue;
            }

            if (entry.timer.tick())
            {
                _pending.emplace_back(std::move(entry));
                continue;
            }

            if (!entry.timer.is_time_up(now))
            {
                _pending.emplace_back(std::move(entry));
                continue;
            }

            try
            {
                dispatch(*entry.message, entry.result);
            }
            catch (const std::exception& ex)
            {
                std::string reason = ex.what();
                entry.result.set_status(ResultStatus::Failed, reason);
                finalize_callbacks(*entry.message, entry.result, ResultStatus::Failed, &reason);
            }
            catch (...)
            {
                std::string reason = "Unknown exception during message dispatch.";
                entry.result.set_status(ResultStatus::Failed, reason);
                finalize_callbacks(*entry.message, entry.result, ResultStatus::Failed, &reason);
            }
        }

        _processing.clear();
    }
}
