#pragma once
#include "Tbx/Systems/Events/Event.h"
#include "Tbx/Systems/Rendering/IRenderer.h"
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


    class CreateRendererRequest : public RenderEvent
    {
    public:
        EXPORT explicit CreateRendererRequest(const std::shared_ptr<IRenderSurface>& surfaceToCreateFor)
            : _surface(surfaceToCreateFor) {}

        EXPORT std::string ToString() const final
        {
            return "Render Frame Request Event";
        }

        EXPORT std::shared_ptr<IRenderSurface> GetSurfaceToCreateFor() const
        {
            return _surface;
        }

        EXPORT void SetResult(std::shared_ptr<IRenderer> newRenderer)
        {
            _result = newRenderer;
        }

        EXPORT std::shared_ptr<IRenderer> GetResult() const
        {
            return _result;
        }

    private:
        std::shared_ptr<IRenderSurface> _surface;
        std::shared_ptr<IRenderer> _result;
    };
}