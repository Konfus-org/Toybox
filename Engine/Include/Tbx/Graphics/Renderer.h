#pragma once
#include "Tbx/Graphics/GraphicsContext.h"
#include "Tbx/Graphics/GraphicsApi.h"
#include "Tbx/Graphics/DrawCommands.h"
#include "Tbx/Memory/Refs.h"

namespace Tbx
{
    /// <summary>
    /// Deals with drawing things to the screen via draw commands.
    /// </summary>
    class TBX_EXPORT IRenderer
    {
    public:
        virtual ~IRenderer() = default;

        /// <summary>
        /// Draws a frame.
        /// </summary>
        virtual void Draw(const DrawCommandBuffer& buffer) = 0;

        /// <summary>
        /// Clears screen and flushes data passed to the GPU.
        /// </summary>
        virtual void Flush() = 0;

        /// <summary>
        /// Gets the graphics Api the renderer uses.
        /// </summary>
        virtual GraphicsApi GetApi() const = 0;
    };

    /// <summary>
    /// Factory for creating renderers for a set of supported graphics APIs.
    /// </summary>
    class TBX_EXPORT IRendererFactory : public IGraphicsPipe
    {
    public:
        virtual ~IRendererFactory() = default;
        virtual Ref<IRenderer> Create(Ref<IGraphicsContext> context) = 0;
    };
}
