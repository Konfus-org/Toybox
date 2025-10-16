#pragma once
#include "Tbx/Debug/IPrintable.h"
#include "Tbx/DllExport.h"

namespace Tbx
{
    /// <summary>
    /// Base for all events.
    /// When creating a new event, inherit from this class and implement the GetCategorization method.
    /// In general all events should be named using past tense, ex: CoolThingHappenedEvent.
    /// If the event is marked as handled, the event coordinator will stop processing it and not send it to any other subscribers.
    /// </summary>
    struct TBX_EXPORT Event
    {
    public:
        virtual ~Event() = default;
        
        /// <summary>
        /// Mark the event as handled. 
        /// This will stop the event coordinator from sending it to any other subscribers.
        /// </summary>
        bool IsHandled = false;
    };
}