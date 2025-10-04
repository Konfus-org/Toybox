#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Graphics/GraphicsApi.h"
#include "Tbx/Memory/Refs.h"

namespace Tbx
{
    class Window;

    using ProcAddressFunPtr = void*;

    /// <summary>
    /// Represents a graphics context responsible for bootstrapping and managing a rendering API.
    /// </summary>
    class TBX_EXPORT IGraphicsContext
    {
    public:

        virtual ~IGraphicsContext() = default;

        /// <summary>
        /// Gets the function pointer used to load graphics functions at runtime;
        /// </summary>
        virtual ProcAddressFunPtr GetProcAddressLoader() = 0;

        /// <summary>
        /// Makes the context active on the current thread.
        /// </summary>
        virtual void MakeCurrent() = 0;

        /// <summary>
        /// Presents the rendered contents to the screen.
        /// </summary>
        virtual void SwapBuffers() = 0;

        /// <summary>
        /// Sets the swap interval for the context.
        /// </summary>
        /// <param name="interval"></param>
        virtual void SetSwapInterval(int interval) = 0;
    };

    /// <summary>
    /// Creates graphics contexts for a given window and graphics API.
    /// </summary>
    class TBX_EXPORT IGraphicsContextProvider : public IUseGraphicsApis
    {
    public:
        virtual ~IGraphicsContextProvider() = default;

        /// <summary>
        /// Retrieves a graphics context for the provided window using the requested API.
        /// </summary>
        virtual Ref<IGraphicsContext> Provide(Ref<Window> window, GraphicsApi api) = 0;
    };
}
