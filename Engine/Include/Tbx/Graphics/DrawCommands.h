#pragma once
#include "Tbx/Graphics/Viewport.h"
#include "Tbx/Graphics/GraphicsUploader.h"
#include "Tbx/Memory/Refs.h"
#include "Tbx/Stages/Stage.h"
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

    using DrawCmdPayloadData = std::variant<Size, Viewport, RgbaColor, ShaderUniform, Ref<GraphicsHandle>>;

    struct DrawCommandPayload
    {
        DrawCommandPayload() = default;
        DrawCommandPayload(const DrawCmdPayloadData& data)
        {
            Data = data;
        }

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
        void AddToyDrawCommandsToBuffer(const Ref<Toy>& toy, DrawCommandBuffer& buffer);
    };

}