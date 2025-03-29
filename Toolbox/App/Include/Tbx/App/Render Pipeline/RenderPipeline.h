#pragma once
#include "Tbx/App/Render Pipeline/RenderCommands.h"
#include "Tbx/App/Render Pipeline/IRenderer.h"
#include "Tbx/App/Render Pipeline/RenderQueue.h"
#include "Tbx/App/Events/ApplicationEvents.h"
#include "Tbx/App/Events/WindowEvents.h"
#include "Tbx/App/Windowing/IWindow.h"
#include <Tbx/Core/Ids/UID.h>

namespace Tbx
{
    class RenderPipeline
    {
    public:
        EXPORT static void Initialize();
        EXPORT static void Shutdown();

        EXPORT static void SetVSyncEnabled(bool enabled);
        EXPORT static bool IsVSyncEnabled();

        EXPORT static void Push(const RenderCommand& command, const std::any& data = nullptr);
        EXPORT static void Clear();
        EXPORT static void Flush();

    private:
        static bool _vsyncEnabled;
        static UID _focusedWindowId;
        static UID _appUpdatedEventId;
        static UID _windowResizeEventId;
        static UID _windowFocusChangedEventId;
        static RenderQueue _renderQueue;

        static void ProcessNextBatch();

        static void OnAppUpdated(const AppUpdatedEvent& e);
        static void OnWindowFocusChanged(const WindowFocusChangedEvent& e);
        static void OnWindowResize(const WindowResizedEvent& e);
    };
}
