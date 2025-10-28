#pragma once
#include "tbx/messages/cancellation_token.h"
#include "tbx/messages/message.h"
#include <chrono>
#include <functional>
#include <optional>

namespace tbx
{
    struct MessageConfiguration
    {
        using Callback = std::function<void(const Message&)>;

        Callback on_failure;
        Callback on_cancelled;
        Callback on_handled;
        Callback on_processed;

        std::optional<std::size_t> delay_ticks;
        std::optional<std::chrono::steady_clock::duration> delay_time;

        CancellationToken cancellation_token;
    };
}
