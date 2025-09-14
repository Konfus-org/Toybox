#pragma once
#include "Tbx/Graphics/IRenderSurface.h"
#include "Tbx/Graphics/Buffers.h"
#include "Tbx/Graphics/Color.h"
#include "Tbx/Graphics/GraphicsApi.h"
#include "Tbx/Math/Vectors.h"
#include <memory>

namespace Tbx
{
    struct Viewport
    {
        Vector2 Position;
        Size Extends;
    };

    class EXPORT IRenderer
    {
    public:
        virtual ~IRenderer() = default;

        /// <summary>
        /// Initializes the renderer.
        /// </summary>
        virtual void Initialize(const std::shared_ptr<IRenderSurface>& surface) = 0;

        /// <summary>
        /// Draws a frame.
        /// </summary>
        virtual void Process(const FrameBuffer& buffer) = 0;

        /// <summary>
        /// Clears screen and flushes data passed to the GPU.
        /// </summary>
        virtual void Flush() = 0;

        /// <summary>
        /// Clears the screen with the given color.
        /// </summary>
        virtual void Clear(const RgbaColor& color = Consts::Colors::DarkGrey) = 0;

        /// <summary>
        /// Sets the api the renderer will use.
        /// </summary>
        virtual void SetApi(GraphicsApi api) = 0;

        /// <summary>
        /// Gets the api the renderer is using.
        /// </summary>
        virtual GraphicsApi GetApi() = 0;

        /// <summary>
        /// Sets the viewport for the renderer.
        /// The viewport is the area of the screen where the renderer will draw.
        /// </summary>
        virtual void SetViewport(const Viewport& viewport) = 0;

        /// <summary>
        /// Gets the viewport the renderer is using.
        /// The viewport is the area of the screen where the renderer will draw.
        /// </summary>
        virtual Viewport GetViewport() = 0;

        /// <summary>
        /// Sets the resolution for the renderer, this is seperate from the viewport size.
        /// </summary>
        virtual void SetResolution(const Size& size) = 0;

        /// <summary>
        /// Gets the resolution used by the renderer, this is seperate from the viewport size.
        /// </summary>
        virtual Size GetResolution() = 0;

        /// <summary>
        /// Sets the VSync for the renderer.
        /// Vsync is used to synchronize the frame rate of the renderer with the refresh rate of the monitor.
        /// </summary>
        virtual void SetVSyncEnabled(bool enabled) = 0;

        /// <summary>
        /// Sets the VSync for the renderer.
        /// Vsync is used to synchronize the frame rate of the renderer with the refresh rate of the monitor.
        /// </summary>
        virtual bool GetVSyncEnabled() = 0;
    };

    class EXPORT IRendererFactory
    {
    public:
        virtual ~IRendererFactory() = default;
        virtual std::shared_ptr<IRenderer> Create(std::shared_ptr<IRenderSurface> surface) = 0;
    };
}
