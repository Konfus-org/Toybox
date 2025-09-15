#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Layers/Layer.h"
#include "Tbx/ECS/ThreeDSpace.h"
#include "Tbx/Graphics/IRenderer.h"
#include "Tbx/Windowing/IWindow.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Events/WindowEvents.h"
#include "Tbx/Events/AppEvents.h"
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
            std::shared_ptr<ThreeDSpace> worldSpace,
            std::shared_ptr<IRendererFactory> renderFactory,
            std::shared_ptr<EventBus> eventBus);

        /// <summary>
        /// Gets the renderer used by a given window.
        /// </summary>
        EXPORT std::shared_ptr<IRenderer> GetRenderer(const std::shared_ptr<IWindow>& window);

    protected:
        void OnUpdate() override;

    private:
        void DrawFrame();
        void OnWindowOpened(const WindowOpenedEvent& e);
        void OnWindowClosed(const WindowClosedEvent& e);
        void OnAppSettingsChanged(const AppSettingsChangedEvent& e);

    private:
        std::vector<std::shared_ptr<IWindow>> _windows = {};
        std::vector<std::shared_ptr<IRenderer>> _renderers = {};
        std::shared_ptr<IRendererFactory> _renderFactory = {};
        std::shared_ptr<ThreeDSpace> _worldSpace = {};
        std::shared_ptr<EventBus> _eventBus = {};
        Tbx::RgbaColor _clearColor = {};
        bool _firstFrame = true;
    };
}