#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/TSS/Stage.h"
#include "Tbx/Layers/Layer.h"
#include "Tbx/Graphics/IRenderer.h"
#include "Tbx/Windowing/IWindow.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Events/WindowEvents.h"
#include "Tbx/Events/AppEvents.h"
#include "Tbx/Memory/Refs.h"
#include <vector>

namespace Tbx
{
    /// <summary>
    /// The apps rendering layer.
    /// Deals with rendering all the items in the apps 3d space.
    /// </summary>
    class RenderingLayer : public Layer
    {
    public:
        EXPORT RenderingLayer(
            Ref<IRendererFactory> renderFactory,
            Ref<EventBus> eventBus);

        /// <summary>
        /// Gets the renderer used by a given window.
        /// </summary>
        EXPORT Ref<IRenderer> GetRenderer(const Ref<IWindow>& window);

    protected:
        void OnUpdate() override;

    private:
        void DrawFrame();
        void OnWindowOpened(const WindowOpenedEvent& e);
        void OnWindowClosed(const WindowClosedEvent& e);
        void OnAppSettingsChanged(const AppSettingsChangedEvent& e);

    private:
        // TODO: listen for the open/close stage event and add/remove open stages here
        std::vector<Ref<Stage>> _openStages = {};
        std::vector<Ref<IWindow>> _windows = {};
        std::vector<Ref<IRenderer>> _renderers = {};
        Ref<IRendererFactory> _renderFactory = {};
        Ref<EventBus> _eventBus = {};
        Tbx::RgbaColor _clearColor = {};
        bool _firstFrame = true;
    };
}