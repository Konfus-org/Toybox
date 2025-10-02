#pragma once
#include "Tbx/Graphics/RenderCommands.h"
#include "Tbx/Graphics/GraphicsContext.h"
#include "Tbx/Memory/Refs.h"
#include <vector>

namespace Tbx
{
    class TBX_EXPORT IRenderer
    {
    public:
        virtual ~IRenderer() = default;

        /// <summary>
        /// Draws a frame.
        /// </summary>
        virtual void Process(const RenderCommandBuffer& buffer) = 0;

        /// <summary>
        /// Clears screen and flushes data passed to the GPU.
        /// </summary>
        virtual void Flush() = 0;

        /// <summary>
        /// Gets the graphics Api the renderer uses.
        /// </summary>
        virtual GraphicsApi GetApi() const = 0;
    };

    class TBX_EXPORT IRendererFactory
    {
    public:
        virtual ~IRendererFactory() = default;
        virtual std::vector<GraphicsApi> GetSupportedApis() const = 0;
        virtual Ref<IRenderer> Create(Ref<IGraphicsContext> context) = 0;
    };
}
