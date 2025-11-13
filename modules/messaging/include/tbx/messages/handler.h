#pragma once
#include "tbx/messages/message.h"
#include <functional>

namespace tbx
{
    // Subscriber callback invoked by the message coordinator during dispatch.
    // Ownership: non-owning. The coordinator stores a copy of the callable.
    // Thread-safety: Invoked on the coordinator's calling thread; handlers
    // should avoid blocking and must manage their own synchronization if
    // touching shared state.
    using MessageHandler = std::function<void(Message&)>;
}
