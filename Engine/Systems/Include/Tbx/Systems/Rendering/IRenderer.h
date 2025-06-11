#pragma once
#include "Tbx/Pipelines/Rendering/IRenderSurface.h"
#include "Tbx/Pipelines/Rendering/FrameBuffer.h"
#include "Tbx/Graphics/Color.h"
#include "Tbx/Graphics/Shader.h"
#include "Tbx/Graphics/Texture.h"
#include "Tbx/Graphics/Mesh.h"

namespace Tbx
{
    struct Viewport
    {
        Vector2 Position;
        Size Size;
    };

    class EXPORT IRenderer
    {
    public:
        virtual ~IRenderer() = default;

        /// <summary>
        /// Initializes the renderer.
        /// </summary>
        virtual void Initialize(const std::shared_ptr<IRenderSurface>& context) = 0;

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
        virtual const Viewport& GetViewport() = 0;

        /// <summary>
        /// Sets the resolution for the renderer, this is seperate from the viewport size.
        /// </summary>
        virtual void SetResolution(const Size& size) = 0;

        /// <summary>
        /// Gets the resolution used by the renderer, this is seperate from the viewport size.
        /// </summary>
        virtual const Size& GetResolution() = 0;

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

        /// <summary>
        /// Clears screen and flushes data passed to the GPU.
        /// </summary>
        virtual void Flush() = 0;

        /// <summary>
        /// Clears the screen with the given color.
        /// </summary>
        virtual void Clear(const Color& color = Colors::DarkGrey) = 0;

        /// <summary>
        /// Call this before drawing anything to the screen.
        /// It must be called in a pair with EndDraw() with end draw called after all drawing is done.
        /// </summary>
        virtual void BeginDraw() = 0;

        /// <summary>
        /// Call this after drawing everything to the screen.
        /// It must be called in a pair with BeginDraw() with begin draw called before drawing anything.
        /// </summary>
        virtual void EndDraw() = 0;

        /// <summary>
        /// Draws a frame.
        /// </summary>
        virtual void Draw(const FrameBuffer& buffer) = 0;
    };
}
