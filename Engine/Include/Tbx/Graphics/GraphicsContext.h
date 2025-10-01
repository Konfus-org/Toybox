#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Graphics/GraphicsApi.h"
#include "Tbx/Memory/Refs.h"

namespace Tbx
{
    class Window;

    /// <summary>
    /// Represents a graphics context responsible for bootstrapping and managing a rendering API.
    /// </summary>
    class TBX_EXPORT IGraphicsConfig
    {
    public:
        virtual ~IGraphicsConfig() = default;

        /// <summary>
        /// Makes the context active on the current thread.
        /// </summary>
        virtual void MakeCurrent() = 0;

        /// <summary>
        /// Presents the rendered contents to the screen.
        /// </summary>
        virtual void SwapBuffers() = 0;

        /// <summary>
        /// Returns the graphics API supported by this context.
        /// </summary>
        virtual GraphicsApi GetApi() const = 0;
    };

    /// <summary>
    /// Creates graphics contexts for a given window and graphics API.
    /// </summary>
    class TBX_EXPORT IGraphicsConfigProvider
    {
    public:
        virtual ~IGraphicsConfigProvider() = default;

        /// <summary>
        /// Retrieves a graphics context for the provided window using the requested API.
        /// </summary>
        virtual Ref<IGraphicsConfig> Get(const Ref<Window>& window, GraphicsApi api) = 0;
    };
}
