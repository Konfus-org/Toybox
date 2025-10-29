#pragma once
#include "tbx/core/cancellation_token.h"
#include "tbx/messages/message.h"
#include "tbx/time/time_span.h"
#include <cstddef>
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
        std::optional<TimeSpan> delay_time;

        CancellationToken cancellation_token;
    };
}
