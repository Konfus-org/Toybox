#pragma once
#include "EventCategory.h"

#define TBX_BIND_EVENT_FN(fn) [this](auto&&... args) -> decltype(auto) { return this->fn(std::forward<decltype(args)>(args)...); }

namespace Toybox
{
    class Event
    {
    public:
        Event() = default;
        virtual ~Event() = default;

        virtual int GetCategorization() const = 0;
        virtual std::string GetName() const = 0;
        inline bool IsInCategory(EventCategory category) const
        {
            return GetCategorization() & category;
        }
        
        bool Handled = false;
    };
}