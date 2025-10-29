#pragma once
#include "tbx/ids/uuid.h"

namespace tbx
{
    class MessageResult;

    // Base polymorphic message type for dispatching.
    struct Message
    {
        virtual ~Message() = default;

        Uuid id = Uuid::generate();
        bool is_handled = false;
        MessageResult* result = nullptr;
    };
}

