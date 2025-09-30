#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Stages/Views.h"
#include "Tbx/Graphics/Material.h"
#include "Tbx/Graphics/Mesh.h"
#include "Tbx/Graphics/Model.h"
#include <any>

namespace Tbx
{
    /// <summary>
    /// The type of render command AKA what the renderer should do.
    /// </summary>
    enum class TBX_EXPORT RenderCommandType
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
        /// Uploads a material's shaders and textures to the GPU.
        /// </summary>
        UploadMaterial,
        /// <summary>
        /// Sets the material to use for rendering.
        /// </summary>
        SetMaterial,
        /// <summary>
        /// Sets a uniform for the GPU (things like view matrix, model position, color, etc..).
        /// </summary>
        SetUniform,
        /// <summary>
        /// Uploads a mesh to the GPU.
        /// </summary>
        UploadMesh,
        /// <summary>
        /// Renders a mesh.
        /// </summary>
        DrawMesh,
        /// <summary>
        /// Sets render viewport.
        /// </summary>
        SetViewport,
        /// <summary>
        /// Sets render resolution.
        /// </summary>
        SetResolution,
    };

    /// <summary>
    /// A render command is an instruction type and a payload for a renderer.
    /// </summary>
    struct TBX_EXPORT RenderCommand
    {
        RenderCommandType Type = RenderCommandType::None;
        std::any Payload = nullptr;
    };

    struct TBX_EXPORT RenderCommandBuffer
    {
        std::vector<RenderCommand> Commands = {};
    };

    /// <summary>
    /// Builds frame buffers of render commands from stages and their toys.
    /// </summary>
    class TBX_EXPORT RenderCommandBufferBuilder
    {
    public:
        RenderCommandBuffer BuildUploadBuffer(StageView<MaterialInstance, Mesh, Model> view);
        RenderCommandBuffer BuildRenderBuffer(FullStageViewView view);

    private:
        void AddToyUploadCommandsToBuffer(const Ref<Toy>& toy, RenderCommandBuffer& buffer);
        void AddToyRenderCommandsToBuffer(const Ref<Toy>& toy, RenderCommandBuffer& buffer);
    };
}