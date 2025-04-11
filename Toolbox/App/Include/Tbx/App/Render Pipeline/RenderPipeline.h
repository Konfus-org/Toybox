#pragma once
#include "Tbx/App/Render Pipeline/RenderQueue.h"
#include "Tbx/App/Events/ApplicationEvents.h"
#include "Tbx/App/Events/WindowEvents.h"
#include "Tbx/App/Windowing/IWindow.h"
#include "Tbx/App/Render Pipeline/RenderProcessor.h"
#include <Tbx/Core/Rendering/RenderData.h>
#include <Tbx/Core/Ids/UID.h>

namespace Tbx
{
    class RenderPipeline
    {
    public:
        EXPORT static void Initialize();
        EXPORT static void Shutdown();

        EXPORT static void SetContext(const std::shared_ptr<Playspace>& currentPlayspace);

        EXPORT static void SetVSyncEnabled(bool enabled);
        EXPORT static bool IsVSyncEnabled();

        EXPORT static void Push(const RenderData& data);
        EXPORT static void Clear();
        EXPORT static void Flush();

    private:
        static void ProcessNextBatch();
        static void OnAppUpdated(const AppUpdatedEvent& e);

        static UID _appUpdatedEventId;

        static std::shared_ptr<Playspace> _currentPlayspace;
        static RenderProcessor _renderProcessor;
        static RenderQueue _renderQueue;

        static bool _vsyncEnabled;
    };
}
