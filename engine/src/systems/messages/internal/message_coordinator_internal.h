#pragma once
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/messaging/message_coordinator.h"
#include <algorithm>
#include <exception>
#include <mutex>
#include <string>
#include <utility>

namespace tbx::internal
{
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
}
