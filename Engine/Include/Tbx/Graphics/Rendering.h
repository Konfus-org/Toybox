#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Graphics/IRenderer.h"
#include "Tbx/Windowing/IWindow.h"
#include "Tbx/TSS/Stage.h"
#include "Tbx/Events/AppEvents.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Events/RenderEvents.h"
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
    class Rendering
    {
    public:
        EXPORT Rendering(Tbx::Ref<IRendererFactory> rendererFactory, Tbx::Ref<EventBus> eventBus);
        EXPORT ~Rendering();

        /// <summary>
        /// Drives the rendering pipeline for all open stages and windows.
        /// </summary>
        EXPORT void Update();

        /// <summary>
        /// Retrieves the renderer associated with the provided window.
        /// </summary>
        EXPORT Tbx::Ref<IRenderer> GetRenderer(const Tbx::Ref<IWindow>& window) const;

    private:
        void DrawFrame();

        /// <summary>
        /// Marks the provided stage for GPU resource upload on the next frame.
        /// </summary>
        void QueueStageUpload(const Tbx::Ref<Stage>& stage);

        /// <summary>
        /// Uploads any pending stage resources to all active renderers.
        /// </summary>
        void FlushPendingUploads();
        void AddStage(const Tbx::Ref<Stage>& stage);
        void RemoveStage(const Tbx::Ref<Stage>& stage);

        void OnWindowOpened(const WindowOpenedEvent& e);
        void OnWindowClosed(const WindowClosedEvent& e);
        void OnAppSettingsChanged(const AppSettingsChangedEvent& e);
        void OnStageOpened(const StageOpenedEvent& e);
        void OnStageClosed(const StageClosedEvent& e);

    private:
        std::vector<Tbx::Ref<Stage>> _openStages = {};
        std::vector<Tbx::Ref<IWindow>> _windows = {};
        std::vector<Tbx::Ref<IRenderer>> _renderers = {};
        std::vector<Tbx::Ref<Stage>> _pendingUploadStages = {};
        Tbx::Ref<IRendererFactory> _rendererFactory = {};
        Tbx::Ref<EventBus> _eventBus = {};
        Tbx::RgbaColor _clearColor = {};
    };
}

