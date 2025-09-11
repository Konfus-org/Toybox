#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Ids/UID.h"
#include "Tbx/ECS/ThreeDSpace.h"
#include "Tbx/Graphics/IRenderer.h"
#include "Tbx/Graphics/IRenderSurface.h"
#include "Tbx/Windowing/IWindow.h"
#include "Tbx/Events/WindowEvents.h"
#include "Tbx/Events/WorldEvents.h"
#include "Tbx/Events/AppEvents.h"
#include "Tbx/PluginAPI/PluginInterfaces.h"
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
        EXPORT explicit(false) RenderingLayer(const std::weak_ptr<App> app);
        EXPORT ~RenderingLayer();

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
        void OnWindowResized(const WindowResizedEvent& e);
        void OnAppSettingsChanged(const AppSettingsChangedEvent& e);

        std::vector<std::shared_ptr<IWindow>> _windows = {};
        std::vector<std::shared_ptr<IRenderer>> _renderers = {};
        std::weak_ptr<IRendererFactoryPlugin> _renderFactory = {};
        Tbx::RgbaColor _clearColor = {};
        bool _firstFrame = true;
    };
}