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
        EXPORT explicit RenderFrameRequest(const RenderBatch& batch)
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

    class EXPORT SetVSyncRequest : public RenderEvent
    {
    public:
        explicit SetVSyncRequest(const bool& vsync)
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