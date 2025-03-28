#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/Events/EventCategory.h"
#include "Tbx/Core/Debug/ILoggable.h"

namespace Tbx
{
    class EXPORT Event : public ILoggable
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