#pragma once
#include "Tbx/Graphics/RenderCommand.h"
#include "Tbx/Graphics/GraphicsApi.h"
#include "Tbx/Memory/Refs.h"

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
        virtual GraphicsApi GetApi() = 0;
    };

    class TBX_EXPORT IOpenGlRenderer : public IRenderer
    {
    public:
        virtual ~IOpenGlRenderer() = default;

        GraphicsApi GetApi() override
        {
            return GraphicsApi::OpenGL;
        }
    };

    class TBX_EXPORT IVulkanRenderer : IRenderer
    {
    public:
        virtual ~IVulkanRenderer() = default;

        GraphicsApi GetApi() override
        {
            return GraphicsApi::Vulkan;
        }
    };

    class TBX_EXPORT IRendererFactory
    {
    public:
        virtual ~IRendererFactory() = default;
        virtual Ref<IRenderer> Create() = 0;
    };
}
