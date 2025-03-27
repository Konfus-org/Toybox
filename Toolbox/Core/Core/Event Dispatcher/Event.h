#pragma once
#include "Core/ToolboxAPI.h"
#include "Core/Event Dispatcher/EventCategory.h"
#include "Core/Debug/ILoggable.h"

namespace Tbx
{
    class TBX_API Event : public ILoggable
    {
    public:
        Event() = default;
        ~Event() override = default;

        virtual std::string ToString() const = 0;
        virtual int GetCategorization() const = 0;
        bool IsInCategory(EventCategory category) const
        {
            return GetCategorization() & static_cast<int>(category);
        }
        
        bool Handled = false;
    };
}