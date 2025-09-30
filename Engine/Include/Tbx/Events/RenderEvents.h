#pragma once
#include "Tbx/Events/Event.h"
#include "Tbx/DllExport.h"
#include <string>

namespace Tbx
{
    class TBX_EXPORT RenderedFrameEvent final : public Event
    {
    public:
        std::string ToString() const override
        {
            return "Rendered Frame Event";
        }
    };
}