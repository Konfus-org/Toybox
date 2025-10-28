#include "tbx/messages/coordinator.h"
#include "tbx/memory/smart_pointers.h"
#include <chrono>

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

    MessageResult MessageCoordinator::dispatch(Message& msg, const MessageConfiguration& config, MessageResult result) const
    {
        if (config.cancellation_token && config.cancellation_token.is_cancelled())
        {
            finalize_callbacks(msg, config, result, MessageStatus::Cancelled);
            return result;
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
        if (msg.is_handled)
            status = MessageStatus::Handled;
        else if (handler_invoked)
            status = MessageStatus::Failed;

        finalize_callbacks(msg, config, result, status);
        return result;
    }

    void MessageCoordinator::finalize_callbacks(const Message& msg, const MessageConfiguration& config, MessageResult& result, MessageStatus status) const
    {
        result.set_status(status);

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
        Message& mutable_msg = const_cast<Message&>(msg);
        return dispatch(mutable_msg, config, result);
    }

    MessageResult MessageCoordinator::post(const Message& msg, const MessageConfiguration& config)
    {
        struct Copy final : Message { Copy(const Message& m) { *static_cast<Message*>(this) = m; } };

        MessageResult result;

        QueuedMessage queued;
        queued.message = make_scope<Copy>(msg);
        queued.config = config;
        queued.result = result;
        if (config.delay_ticks)
        {
            queued.has_tick_delay = true;
            queued.remaining_ticks = *config.delay_ticks;
        }
        if (config.delay_time)
        {
            queued.has_time_delay = true;
            queued.ready_time = std::chrono::steady_clock::now() + *config.delay_time;
        }

        _pending.emplace_back(std::move(queued));
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
                finalize_callbacks(*entry.message, entry.config, entry.result, MessageStatus::Cancelled);
                continue;
            }

            if (entry.has_tick_delay && entry.remaining_ticks > 0)
            {
                --entry.remaining_ticks;
                _pending.emplace_back(std::move(entry));
                continue;
            }

            if (entry.has_time_delay && now < entry.ready_time)
            {
                _pending.emplace_back(std::move(entry));
                continue;
            }

            entry.result = dispatch(*entry.message, entry.config, std::move(entry.result));
        }

        _processing.clear();
    }
}
