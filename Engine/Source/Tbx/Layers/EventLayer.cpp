#include "Tbx/PCH.h"
#include "Tbx/Layers/EventLayer.h"

namespace Tbx
{
    void EventLayer::OnDetach()
    {
        EventCoordinator::ClearSubscribers();
    }
}
