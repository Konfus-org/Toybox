#pragma once
#include "tbx/messages/message.h"
#include <functional>

namespace tbx
{
    using MessageHandler = std::function<void(const Message&)>;
}

