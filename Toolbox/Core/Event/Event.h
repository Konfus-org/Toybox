#pragma once
#include "TbxAPI.h"
#include "EventCategory.h"

namespace Tbx
{
    class TBX_API Event
    {
    public:
        Event() = default;
        virtual ~Event() = default;

        virtual std::string ToString() const = 0;
        virtual int GetCategorization() const = 0;
        bool IsInCategory(EventCategory category) const
        {
            return GetCategorization() & static_cast<int>(category);
        }
        
        bool Handled = false;
    };

    using EventCallbackFn = TBX_API std::function<void(Event&)>;
}