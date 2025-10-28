#pragma once
#include "tbx/ids/uuid.h"

namespace tbx
{
    // Base polymorphic message type for dispatching.
    struct Message
    {
        virtual ~Message() = default;

        Uuid id = Uuid::generate();
        bool is_handled = false;
    };
}

