#include "Tbx/PCH.h"
#include "Tbx/Layers/EventCoordinatorLayer.h"
#include "Tbx/Events/EventCoordinator.h"

namespace Tbx
{
    bool EventCoordinatorLayer::IsOverlay()
    {
        return false;
    }

    void EventCoordinatorLayer::OnAttach()
    {
        // Do nothing...
    }

    void EventCoordinatorLayer::OnDetach()
    {
        EventCoordinator::ClearSubscribers();
    }

    void EventCoordinatorLayer::OnUpdate()
    {
        // Do nothing...
    }
}
