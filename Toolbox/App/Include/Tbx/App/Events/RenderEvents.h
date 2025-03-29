#pragma once
#include "Tbx/App/Render Pipeline/RenderQueue.h"
#include <Tbx/Core/DllExport.h>
#include <Tbx/Core/Events/Event.h>
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

    class EXPORT RenderClearFrameRequestEvent : public RenderEvent
    {
    public:
        std::string ToString() const final
        {
            return "ClearFrameEvent";
        }
    };

    class EXPORT BeginRenderFrameRequestEvent : public RenderEvent
    {
    public:
        std::string ToString() const final
        {
            return "BeginRenderFrameRequestEvent";
        }
    };

    class EXPORT EndRenderFrameRequestEvent : public RenderEvent
    {
    public:
        std::string ToString() const final
        {
            return "EndRenderFrameRequestEvent";
        }
    };

    class EXPORT RenderFrameRequestEvent : public RenderEvent
    {
    public:
        explicit RenderFrameRequestEvent(const RenderBatch& batch)
            : _batch(batch) {
        }

        std::string ToString() const final
        {
            return "RenderFrameRequestEvent";
        }

        RenderBatch& GetBatch() { return _batch; }

    private:
        RenderBatch _batch;
    };

    class EXPORT RenderSetContextRequestEvent : public RenderEvent
    {
    public:
        explicit RenderSetContextRequestEvent(const std::weak_ptr<IWindow>& context)
            : _renderContext(_renderContext) {
        }

        std::string ToString() const final
        {
            return "SetRenderContextRequestEvent";
        }
    private:
        std::weak_ptr<IWindow> _renderContext;
    };

    class EXPORT RenderSetVSyncRequestEvent : public RenderEvent
    {
    public:
        explicit RenderSetVSyncRequestEvent(const bool& vsync)
            : _vSync(vsync) {
        }

        std::string ToString() const final
        {
            return "RenderSetVSyncRequestEvent";
        }

        bool GetVSync() const { return _vSync; }

    private:
        bool _vSync;
    };
}