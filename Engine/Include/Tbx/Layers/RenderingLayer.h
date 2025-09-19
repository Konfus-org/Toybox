#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Layers/Layer.h"
#include "Tbx/Graphics/IRenderer.h"
#include "Tbx/Windowing/IWindow.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Events/WindowEvents.h"
#include "Tbx/Events/AppEvents.h"
#include "Tbx/Memory/Refs.h"
#include <vector>
#include <memory>

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
            Tbx::Ref<IRendererFactory> renderFactory,
            Tbx::Ref<EventBus> eventBus);

        /// <summary>
        /// Gets the renderer used by a given window.
        /// </summary>
        EXPORT Tbx::Ref<IRenderer> GetRenderer(const Tbx::Ref<IWindow>& window);

    protected:
        void OnUpdate() override;

    private:
        void DrawFrame();
        void OnWindowOpened(const WindowOpenedEvent& e);
        void OnWindowClosed(const WindowClosedEvent& e);
        void OnAppSettingsChanged(const AppSettingsChangedEvent& e);

    private:
        std::vector<Tbx::Ref<IWindow>> _windows = {};
        std::vector<Tbx::Ref<IRenderer>> _renderers = {};
        Tbx::Ref<IRendererFactory> _renderFactory = {};
        Tbx::Ref<EventBus> _eventBus = {};
        Tbx::RgbaColor _clearColor = {};
        bool _firstFrame = true;
    };
}