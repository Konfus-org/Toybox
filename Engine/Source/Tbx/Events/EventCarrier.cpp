#include "Tbx/PCH.h"
#include "Tbx/Events/EventCarrier.h"

namespace Tbx
{
    EventCarrier::EventCarrier(Ref<EventBus> bus)
        : _bus(bus)
    {
        TBX_ASSERT(bus, "EventCarrier: Cannot create a carrier without a valid event bus.");
    }
}

