#pragma once
#include "Tbx/Events/Event.h"
#include "Tbx/Graphics/IRenderer.h"
#include "Tbx/Graphics/Buffers.h"
#include "Tbx/DllExport.h"
#include <array>
#include <string>

namespace Tbx
{
    class EXPORT RenderEvent : public Event
    {
    public:
        int GetCategorization() const final
        {
            return static_cast<int>(EventCategory::Render);
        }
    };

    class EXPORT RenderedFrameEvent : public RenderEvent
    {
    public:
        std::string ToString() const final
        {
            return "Rendered Frame Event";
        }
    };
}