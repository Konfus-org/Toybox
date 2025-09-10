#include "Tbx/PCH.h"
#include "Tbx/Layers/EventCoordinatorLayer.h"
#include "Tbx/Events/EventCoordinator.h"

namespace Tbx
{
    void EventCoordinatorLayer::OnDetach()
    {
        EventCoordinator::ClearSubscribers();
    }
}
