#include "Tbx/PCH.h"
#include "Tbx/Events/EventDispatcher.h"

namespace Tbx
{
    EventDispatcher::EventDispatcher(Ref<EventBus> eventBus)
    {
        Bind(eventBus);
    }

    void EventDispatcher::Bind(Ref<EventBus> eventBus)
    {
        _eventBus = eventBus;
    }

    bool EventDispatcher::IsBound() const
    {
        return !_eventBus.expired();
    }

    Ref<EventBus> EventDispatcher::GetEventBus() const
    {
        return LockBus();
    }

    Ref<EventBus> EventDispatcher::LockBus() const
    {
        return _eventBus.lock();
    }
}
