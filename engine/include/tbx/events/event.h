#pragma once
#include "tbx/ids/uuid.h"

namespace tbx
{
    class Event
    {
    public:
        virtual ~Event() = default;

        Uuid id = Uuid::generate();
        bool is_handled = false;
    };
}
