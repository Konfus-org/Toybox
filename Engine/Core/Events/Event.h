#pragma once
#include "TbxPCH.h"
#include "TbxAPI.h"
#include "EventCategory.h"

#define TBX_BIND_EVENT_FN(fn) [this](auto&&... args) -> decltype(auto) { return this->fn(std::forward<decltype(args)>(args)...); }

namespace Tbx
{
    class TBX_API Event
    {
    public:
        Event() = default;
        virtual ~Event() = default;

        virtual int GetCategorization() const = 0;
        virtual std::string GetName() const = 0;
        bool IsInCategory(EventCategory category) const
        {
            return GetCategorization() & static_cast<int>(category);
        }
        
        bool Handled = false;
    };

    using EventCallbackFn = TBX_API std::function<void(Event&)>;
}