#pragma once
#include "Tbx/Utils/DllExport.h"
#include "Tbx/Utils/Ids/UID.h"
#include "Tbx/Systems/Rendering/IRenderer.h"
#include "Tbx/Systems/Rendering/IRenderSurface.h"
#include "Tbx/Systems/Rendering/RenderPipeline.h"
#include "Tbx/Systems/Windowing/WindowEvents.h"
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
        static void OnWindowCreated(const WindowOpenedEvent& event);
        static void OnWindowClosed(const WindowClosedEvent& event);

        static RenderPipeline _pipeline;
        static UID _onWindowCreatedEventId;
        static std::map<UID, std::shared_ptr<IRenderer>> _renderers;
    };
}