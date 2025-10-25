#pragma once
#include "tbx/ids/uuid.h"

namespace tbx
{
    // Base command type for high-frequency dispatch.
    // Concrete commands derive from this and carry their own data.
    class Command
    {
    public:
        virtual ~Command() = default;

        Uuid id = Uuid::generate();
        bool is_handled = false;
    };
}
