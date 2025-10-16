#pragma once
#include "Tbx/Events/Event.h"
#include "Tbx/DllExport.h"
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