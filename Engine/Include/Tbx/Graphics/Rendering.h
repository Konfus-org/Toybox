#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Graphics/IRenderer.h"
#include "Tbx/Windowing/Window.h"
#include "Tbx/Stages/Stage.h"
#include "Tbx/Events/AppEvents.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Events/TSSEvents.h"
#include "Tbx/Events/WindowEvents.h"
#include "Tbx/Memory/Refs.h"
#include "Tbx/Graphics/Color.h"
#include <vector>

namespace Tbx
{
    /// <summary>
    /// Coordinates render targets, windows, and stage composition for a frame.
    /// </summary>
    class TBX_EXPORT Rendering
    {
    public:
        Rendering(Ref<IRendererFactory> rendererFactory, Ref<EventBus> eventBus);
        ~Rendering();

        /// <summary>
        /// Drives the rendering pipeline for all open stages and windows.
        /// </summary>
        void Update();

        /// <summary>
        /// Retrieves the renderer associated with the provided window.
        /// </summary>
        Ref<IRenderer> GetRenderer(const Ref<Window>& window) const;

    private:
        void DrawFrame();
        void ProcessPendingUploads();
        void ProcessOpenStages();

        void QueueStageUpload(const Ref<Stage>& stage);
        void AddStage(const Ref<Stage>& stage);
        void RemoveStage(const Ref<Stage>& stage);

        void OnWindowOpened(const WindowOpenedEvent& e);
        void OnWindowClosed(const WindowClosedEvent& e);
        void OnWindowResized(const WindowResizedEvent& e);
        void OnAppSettingsChanged(const AppSettingsChangedEvent& e);
        void OnStageOpened(const StageOpenedEvent& e);
        void OnStageClosed(const StageClosedEvent& e);

    private:
        std::vector<Ref<Stage>> _openStages = {};
        std::vector<Ref<Window>> _windows = {};
        std::vector<Ref<IRenderer>> _renderers = {};
        std::vector<Ref<Stage>> _pendingUploadStages = {};
        Ref<IRendererFactory> _rendererFactory = {};
        Ref<EventBus> _eventBus = {};
        RgbaColor _clearColor = {};
    };
}

