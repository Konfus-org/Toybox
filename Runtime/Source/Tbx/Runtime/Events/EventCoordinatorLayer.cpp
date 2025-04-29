#include "Tbx/Runtime/PCH.h"
#include "Tbx/Runtime/Events/EventCoordinatorLayer.h"
#include <Tbx/Core/Events/EventCoordinator.h>

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
