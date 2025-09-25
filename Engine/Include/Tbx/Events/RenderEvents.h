#pragma once
#include "Tbx/Events/Event.h"
#include "Tbx/DllExport.h"
#include <string>

namespace Tbx
{
    class TBX_EXPORT RenderedFrameEvent : public Event
    {
    public:
        std::string ToString() const final
        {
            return "Rendered Frame Event";
        }
    };
}