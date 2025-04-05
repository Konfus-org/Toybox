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

    class EXPORT ClearScreenRequestEvent : public RenderEvent
    {
    public:
        std::string ToString() const final
        {
            return "Clear Frame Event";
        }
    };

    class EXPORT FlushRendererRequestEvent : public RenderEvent
    {
    public:
        std::string ToString() const final
        {
            return "Flush Renderer Request Event";
        }
    };

    class EXPORT BeginRenderFrameRequestEvent : public RenderEvent
    {
    public:
        std::string ToString() const final
        {
            return "Begin Render Frame Request Event";
        }
    };

    class EXPORT EndRenderFrameRequestEvent : public RenderEvent
    {
    public:
        std::string ToString() const final
        {
            return "End Render Frame Request Event";
        }
    };

    class RenderFrameRequestEvent : public RenderEvent
    {
    public:
        EXPORT explicit RenderFrameRequestEvent(const RenderBatch& batch)
            : _batch(batch) {}

        EXPORT std::string ToString() const final
        {
            return "Render Frame Request Event";
        }

        EXPORT const RenderBatch& GetBatch() const
        {
            return _batch; 
        }

    private:
        RenderBatch _batch;
    };

    class SetRenderContextRequestEvent : public RenderEvent
    {
    public:
        EXPORT explicit SetRenderContextRequestEvent(const std::weak_ptr<IWindow>& context)
            : _renderContext(context) {}

        EXPORT std::string ToString() const final
        {
            return "Set Render Context Request Event";
        }

        EXPORT std::weak_ptr<IWindow> GetContext() const 
        { 
            return _renderContext; 
        }

    private:
        std::weak_ptr<IWindow> _renderContext;
    };

    class EXPORT SetVSyncRequestEvent : public RenderEvent
    {
    public:
        explicit SetVSyncRequestEvent(const bool& vsync)
            : _vSync(vsync) {
        }

        std::string ToString() const final
        {
            return "Render Set VSync Request Event";
        }

        bool GetVSync() const { return _vSync; }

    private:
        bool _vSync;
    };
}