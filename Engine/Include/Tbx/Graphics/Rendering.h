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
        Rendering(Tbx::Ref<IRendererFactory> rendererFactory, Tbx::Ref<EventBus> eventBus);
        ~Rendering();

        /// <summary>
        /// Drives the rendering pipeline for all open stages and windows.
        /// </summary>
        void Update();

        /// <summary>
        /// Retrieves the renderer associated with the provided window.
        /// </summary>
        Tbx::Ref<IRenderer> GetRenderer(const Tbx::Ref<Window>& window) const;

    private:
        void DrawFrame();
        void ProcessPendingUploads();
        void ProcessOpenStages();

        void QueueStageUpload(const Tbx::Ref<Stage>& stage);
        void AddStage(const Tbx::Ref<Stage>& stage);
        void RemoveStage(const Tbx::Ref<Stage>& stage);

        void OnWindowOpened(const WindowOpenedEvent& e);
        void OnWindowClosed(const WindowClosedEvent& e);
        void OnWindowResized(const WindowResizedEvent& e);
        void OnAppSettingsChanged(const AppSettingsChangedEvent& e);
        void OnStageOpened(const StageOpenedEvent& e);
        void OnStageClosed(const StageClosedEvent& e);

    private:
        std::vector<Tbx::Ref<Stage>> _openStages = {};
        std::vector<Tbx::Ref<Window>> _windows = {};
        std::vector<Tbx::Ref<IRenderer>> _renderers = {};
        std::vector<Tbx::Ref<Stage>> _pendingUploadStages = {};
        Tbx::Ref<IRendererFactory> _rendererFactory = {};
        Tbx::Ref<EventBus> _eventBus = {};
        Tbx::RgbaColor _clearColor = {};
    };
}

