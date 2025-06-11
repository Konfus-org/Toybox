#pragma once
#include "Tbx/Systems/Events/Event.h"
#include "Tbx/Graphics/Buffers.h"
#include "Tbx/Utils/DllExport.h"
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

    class EXPORT ClearScreenRequest : public RenderEvent
    {
    public:
        std::string ToString() const final
        {
            return "Clear Frame Event";
        }
    };

    class EXPORT FlushRendererRequest : public RenderEvent
    {
    public:
        std::string ToString() const final
        {
            return "Flush Renderer Request Event";
        }
    };

    class RenderFrameRequest : public RenderEvent
    {
    public:
        EXPORT explicit RenderFrameRequest(const FrameBuffer& buffer)
            : _buffer(buffer) {
        }

        EXPORT std::string ToString() const final
        {
            return "Render Frame Request Event";
        }

        EXPORT const FrameBuffer& GetBuffer() const
        {
            return _buffer;
        }

    private:
        FrameBuffer _buffer;
    };
}