#pragma once
#include <Tbx/Core/DllExport.h>

namespace Tbx
{
    /// <summary>
    /// A render command is an instruction to the renderer.
    /// Its purpose is to tell the renderer what to do and should be used in conjunction with appropriate data sent to the renderer.
    /// </summary>
    enum class EXPORT RenderCommand
    {
        /// <summary>
        /// Does nothing.
        /// </summary>
        None = 0,
        /// <summary>
        /// Clears the screen.
        /// </summary>
        Clear,
        /// <summary>
        /// Compiles a materials shader(s) and uploads its texture data to the GPU.
        /// </summary>
        CompileMaterial,
        /// <summary>
        /// Uploads data to a materials shader.
        /// </summary>
        UploadMaterialShaderData,
        /// <summary>
        /// Sets the material to use for rendering.
        /// </summary>
        SetMaterial,
        /// <summary>
        /// Renders a mesh.
        /// </summary>
        RenderMesh,
    };
}