#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Ids/UID.h"
#include "Tbx/Graphics/IRenderer.h"
#include "Tbx/Graphics/IRenderSurface.h"
#include "Tbx/Events/WindowEvents.h"
#include "Tbx/Events/WorldEvents.h"
#include "Tbx/PluginAPI/PluginInterfaces.h"
#include <map>

namespace Tbx
{
    class Rendering
    {
    public:
        /// <summary>
        /// Initializes the rendering system.
        /// </summary>
        EXPORT static void Initialize();

        /// <summary>
        /// Shuts down the rendering system.
        /// </summary>
        EXPORT static void Shutdown();

        /// <summary>
        /// Draws a frame for each window.
        /// </summary>
        EXPORT static void DrawFrame();

        /// <summary>
        /// Gets the renderer used by a given window.
        /// </summary>
        EXPORT static std::shared_ptr<IRenderer> GetRenderer(UID window);

    private:
        static void OnWindowOpened(const WindowOpenedEvent& e);
        static void OnWindowClosed(const WindowClosedEvent& e);

        static UID _onWindowCreatedEventId;
        static UID _onWindowClosedEventId;
        static std::map<UID, std::shared_ptr<IRenderer>> _renderers;
        static std::shared_ptr<IRendererFactoryPlugin> _renderFactory;
    };
}