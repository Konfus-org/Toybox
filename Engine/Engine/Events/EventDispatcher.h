#pragma once
#include "Event.h"

namespace Toybox::Events
{
    class EventDispatcher
    {
    public:
        EventDispatcher(Event& event) : m_Event(event) { }
        
        template<typename T, typename F>
        bool Dispatch(const F& func)
        {
            if (typeid(m_Event).name() == typeid(T).name())
            {
                m_Event.Handled |= func(static_cast<T&>(m_Event));
                return true;
            }
            return false;
        }

    private:
        Event& m_Event;
    };
}