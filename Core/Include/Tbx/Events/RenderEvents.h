#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Events/Event.h"
#include <string>

namespace Tbx
{
    struct TBX_EXPORT RenderBeginDrawEvent final : public Event
    {
    };

    struct TBX_EXPORT RenderEndDrawEvent final : public Event
    {
    };

    struct TBX_EXPORT RenderedFrameEvent final : public Event
    {
    };
}
