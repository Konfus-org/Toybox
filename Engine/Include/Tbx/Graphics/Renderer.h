#pragma once
#include "Tbx/Graphics/GraphicsContext.h"
#include "Tbx/Graphics/Viewport.h"
#include "Tbx/Graphics/Material.h"
#include "Tbx/Graphics/Mesh.h"
#include "Tbx/Memory/Refs.h"
#include <vector>

namespace Tbx
{
    /// <summary>
    /// Command type that informs a renderer how to treat a draw command payload.
    /// </summary>
    enum class TBX_EXPORT DrawCommandType
    {
        /// <summary>
        /// Does nothing.
        /// </summary>
        None,
        /// <summary>
        /// Clears the screen.
        /// </summary>
        Clear,
        /// <summary>
        /// Sets render viewport.
        /// </summary>
        SetViewport,
        /// <summary>
        /// Sets render resolution.
        /// </summary>
        SetResolution,
        /// <summary>
        /// Sets the material to use for rendering.
        /// </summary>
        SetMaterial,
        /// <summary>
        /// Sets a uniform for the GPU (things like view matrix, model position, color, etc..).
        /// </summary>
        SetUniform,
        /// <summary>
        /// Renders a mesh.
        /// </summary>
        DrawMesh
    };

    using DrawCmdPayloadData = std::variant<Size, Viewport, RgbaColor, ShaderUniform, Ref<Material>, Ref<Mesh>>;

    struct DrawCommandPayload
    {
        template <typename TPayload>
        const TPayload& As() const
        {
            return std::get<TPayload>(Data);
        }

        DrawCmdPayloadData Data = {};
    };

    /// <summary>
    /// A draw command is an instruction that tells the renderer what, where, and how to draw something.
    /// </summary>
    struct TBX_EXPORT DrawCommand
    {
        DrawCommandType Type = DrawCommandType::None;
        DrawCommandPayload Payload = {};
    };

    /// <summary>
    /// A collection of draw commands that can be sent to the renderer to be drawn.
    /// </summary>
    struct TBX_EXPORT DrawCommandBuffer
    {
        std::vector<DrawCommand> Commands = {};
    };

    /// <summary>
    /// Builds frame buffers of render commands from stages and their toys.
    /// </summary>
    class TBX_EXPORT DrawCommandBufferBuilder
    {
    public:
        DrawCommandBuffer Build(std::vector<Ref<Stage>> stagesToRender, Size viewportSize, RgbaColor clearColor);

    private:
        void AddToyUploadCommandsToBuffer(const Ref<Toy>& toy, DrawCommandBuffer& buffer);
        void AddToyRenderCommandsToBuffer(const Ref<Toy>& toy, DrawCommandBuffer& buffer);
    };

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
    class TBX_EXPORT IRendererFactory
    {
    public:
        virtual ~IRendererFactory() = default;
        virtual std::vector<GraphicsApi> GetSupportedApis() const = 0;
        virtual Ref<IRenderer> Create(Ref<IGraphicsContext> context) = 0;
    };
}
