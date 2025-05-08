#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/Events/EventCategory.h"
#include "Tbx/Core/Debug/IPrintable.h"

namespace Tbx
{
    /// <summary>
    /// Base class for all events.
    /// When creating a new event, inherit from this class and implement the GetCategorization method.
    /// If the event is expected to be responded to once sent (two way), it is recommended to use the Request suffix.
    /// If the event is not expected to be responded to, it is recommended to use the Event suffix.
    /// In general all events should be named using past tense, ex: CoolThingHappenedEvent.
    /// If the event is marked as handled, the event coordinator will stop processing it and not send it to any other subscribers.
    /// </summary>
    class EXPORT Event : public IPrintable
    {
    public:
        virtual ~Event() = default;

        /// <summary>
        /// Gets int value of the event categorization (EventCategory).
        /// </summary>
        virtual int GetCategorization() const = 0;

        /// <summary>
        /// Checks if the event is in the specified category.
        /// </summary>
        bool IsInCategory(EventCategory category) const
        {
            return GetCategorization() & static_cast<int>(category);
        }
        
        /// <summary>
        /// Mark the event as handled. 
        /// This will stop the event coordinator from sending it to any other subscribers.
        /// </summary>
        bool IsHandled = false;
    };
}