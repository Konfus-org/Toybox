#pragma once
#include "Event.h"
#include "tbxAPI.h"

namespace Toybox
{
    class TBX_API EventDispatcher
    {
    public:
        EventDispatcher(Event& event) : m_Event(event) { }
        
        template<typename T, typename F>
        bool Dispatch(const F& func)
        {
            std::string eventName = typeid(m_Event).name();
            std::string inputName = typeid(T).name();
            if (eventName == inputName)
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