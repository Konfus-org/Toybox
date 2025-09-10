#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Ids/UID.h"
#include "Tbx/Graphics/IRenderer.h"
#include "Tbx/Graphics/IRenderSurface.h"
#include "Tbx/Windowing/IWindow.h"
#include "Tbx/Events/WindowEvents.h"
#include "Tbx/Events/WorldEvents.h"
#include "Tbx/Events/AppEvents.h"
#include "Tbx/PluginAPI/PluginInterfaces.h"
#include "Tbx/TBS/World.h"
#include "Tbx/Graphics/GraphicsSettings.h"
#include <vector>
#include <memory>

namespace Tbx
{
    class Rendering
    {
    public:
        EXPORT explicit(false) Rendering();
        EXPORT ~Rendering();

        /// <summary>
        /// Draws a frame for each window.
        /// </summary>
        EXPORT void DrawFrame(const std::shared_ptr<World>& world);

        /// <summary>
        /// Gets the renderer used by a given window.
        /// </summary>
        EXPORT std::shared_ptr<IRenderer> GetRenderer(const std::shared_ptr<IWindow>& window);

    private:
        void OnWindowOpened(const WindowOpenedEvent& e);
        void OnWindowClosed(const WindowClosedEvent& e);
        void OnWindowResized(const WindowResizedEvent& e);
        void OnGraphicsSettingsChanged(const AppGraphicsSettingsChangedEvent& e);

        std::vector<std::shared_ptr<IWindow>> _windows;
        std::vector<std::shared_ptr<IRenderer>> _renderers;
        std::weak_ptr<IRendererFactoryPlugin> _renderFactory;
        bool _firstFrame = true;
        GraphicsSettings _graphicsSettings = {};
    };
}