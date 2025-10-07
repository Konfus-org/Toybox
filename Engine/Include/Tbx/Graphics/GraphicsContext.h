#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Graphics/GraphicsApi.h"
#include "Tbx/Memory/Refs.h"

namespace Tbx
{
    class Window;

    /// <summary>
    /// Represents the available vertical sync modes for a graphics context swap chain.
    /// </summary>
    enum class VsyncMode
    {
        Off,
        On,
        Adaptive
    };

    /// <summary>
    /// Represents a graphics context responsible for managing a windows rendering.
    /// </summary>
    class TBX_EXPORT IGraphicsContext
    {
    public:
        virtual ~IGraphicsContext() = default;

        /// <summary>
        /// Makes the context active on the current thread.
        /// </summary>
        virtual void MakeCurrent() = 0;

        /// <summary>
        /// Presents the rendered contents to the screen.
        /// </summary>
        virtual void Present() = 0;

        /// <summary>
        /// Sets the vsync mode for the context.
        /// </summary>
        virtual void SetVsync(VsyncMode mode) = 0;

    };

    /// <summary>
    /// Creates graphics contexts for a given window and graphics API.
    /// </summary>
    class TBX_EXPORT IGraphicsContextProvider
    {
    public:
        virtual ~IGraphicsContextProvider() = default;

        /// <summary>
        /// Identifies the graphics API handled by this provider.
        /// </summary>
        virtual GraphicsApi GetApi() const = 0;

        /// <summary>
        /// Provides a graphics context for the provided window using the provider's API.
        /// </summary>
        virtual Ref<IGraphicsContext> Provide(Ref<Window> window) = 0;
    };
}
