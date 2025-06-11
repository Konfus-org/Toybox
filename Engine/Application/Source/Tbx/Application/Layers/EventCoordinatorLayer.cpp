#include "Tbx/Application/PCH.h"
#include "Tbx/Application/Layers/EventCoordinatorLayer.h"
#include "Tbx/Systems/Events/EventCoordinator.h"

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
